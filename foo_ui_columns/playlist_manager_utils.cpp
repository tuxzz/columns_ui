#include "stdafx.h"
#include "playlist_manager_utils.h"

playlist_format_name_t::titleformat_hook_playlist_t::titleformat_hook_playlist_t(
    unsigned p_index, const char* p_name, t_size p_playing)
    : m_name(p_name)
    , m_playing(p_playing == p_index)
    , m_index(p_index)
    , m_lock_name_initialised(false)
    , m_length_initialised(false)
    , m_filesize_initialised(false)
    , m_filesize(NULL)
{
    m_metadb_api = static_api_ptr_t<metadb>().get_ptr();
    m_api = static_api_ptr_t<playlist_manager_v3>().get_ptr();
    m_locked = m_api->playlist_lock_is_present(p_index);
    m_size = m_api->playlist_get_item_count(p_index);
    m_active = m_api->get_active_playlist() == p_index;
};

playlist_format_name_t::playlist_format_name_t(unsigned p_index, const char* src, t_size p_playing)
{
    if (cfg_playlist_switcher_use_tagz) {
        service_ptr_t<titleformat_object> to_temp;
        static_api_ptr_t<titleformat_compiler>()->compile_safe(to_temp, cfg_playlist_switcher_tagz);
        titleformat_hook_playlist_t tf_hook(p_index, src, p_playing);
        to_temp->run(&tf_hook, *this, nullptr);
    } else
        set_string(src);
}

bool playlist_format_name_t::titleformat_hook_playlist_t::process_field(
    titleformat_text_out* p_out, const char* p_name, unsigned p_name_length, bool& p_found_flag)
{
    p_found_flag = false;
    if (!stricmp_utf8_ex(p_name, p_name_length, "title", pfc_infinite)) {
        p_out->write(titleformat_inputtypes::unknown, m_name, pfc_infinite);
        p_found_flag = true;
        return true;
    }
    if (!stricmp_utf8_ex(p_name, p_name_length, "is_locked", pfc_infinite)) {
        if (m_locked) {
            p_out->write(titleformat_inputtypes::unknown, "1", pfc_infinite);
            p_found_flag = true;
            return true;
        }
        return false;
    }
    if (!stricmp_utf8_ex(p_name, p_name_length, "is_active", pfc_infinite)) {
        if (m_active) {
            p_out->write(titleformat_inputtypes::unknown, "1", pfc_infinite);
            p_found_flag = true;
            return true;
        }
        return false;
    }
    if (!stricmp_utf8_ex(p_name, p_name_length, "size", pfc_infinite)) {
        p_out->write_int(titleformat_inputtypes::unknown, m_size);
        p_found_flag = true;
        return true;
    }
    if (!stricmp_utf8_ex(p_name, p_name_length, "is_playing", pfc_infinite)) {
        if (m_playing) {
            p_out->write(titleformat_inputtypes::unknown, "1", pfc_infinite);
            p_found_flag = true;
            return true;
        }
    } else if (!stricmp_utf8_ex(p_name, p_name_length, "lock_name", pfc_infinite)) {
        if (m_locked) {
            if (!m_lock_name_initialised) {
                m_api->playlist_lock_query_name(m_index, m_lock_name);
                m_lock_name_initialised = true;
            }
            p_out->write(titleformat_inputtypes::unknown, m_lock_name, pfc_infinite);
            p_found_flag = true;
            return true;
        }
    } else if (!stricmp_utf8_ex(p_name, p_name_length, "length", pfc_infinite)) {
        if (!m_length_initialised) {
            metadb_handle_list_t<pfc::alloc_fast_aggressive> list;
            list.set_count(m_size);
            m_api->playlist_get_all_items(m_index, list);

            m_length = pfc::format_time_ex(list.calc_total_duration(), 0);
            m_length_initialised = true;
        }
        p_out->write(titleformat_inputtypes::unknown, m_length.get_ptr(), pfc_infinite);
        p_found_flag = true;
        return true;
    } else if (!stricmp_utf8_ex(p_name, p_name_length, "filesize_raw", pfc_infinite)) {
        initialise_filesize();
        p_out->write(titleformat_inputtypes::unknown, pfc::format_uint(m_filesize).get_ptr(), pfc_infinite);
        p_found_flag = true;
        return true;
    } else if (!stricmp_utf8_ex(p_name, p_name_length, "filesize", pfc_infinite)) {
        initialise_filesize();
        mmh::FileSizeFormatter str(m_filesize);
        p_out->write(titleformat_inputtypes::unknown, str.get_ptr(), pfc_infinite);
        p_found_flag = true;
        return true;
    }
    return false;
}

namespace playlist_manager_utils {

class rename_param {
public:
    modal_dialog_scope m_scope;
    pfc::string8* m_text{};
};

static BOOL CALLBACK RenameProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowLongPtr(wnd, DWLP_USER, lp);
        {
            auto* ptr = (rename_param*)lp;
            ptr->m_scope.initialize(FindOwningPopup(wnd));
            pfc::string_formatter formatter;
            formatter << R"(Rename playlist: ")" << *ptr->m_text << R"(")";
            uSetWindowText(wnd, formatter);
            uSetDlgItemText(wnd, IDC_EDIT, ptr->m_text->get_ptr());
        }
        return 1;
    case WM_COMMAND:
        switch (wp) {
        case IDOK: {
            auto* ptr = (rename_param*)GetWindowLong(wnd, DWLP_USER);
            uGetDlgItemText(wnd, IDC_EDIT, *ptr->m_text);
            EndDialog(wnd, 1);
        } break;
        case IDCANCEL:
            EndDialog(wnd, 0);
            break;
        }
        break;
    case WM_CLOSE:
        EndDialog(wnd, 0);
        break;
    }
    return 0;
}

static bool rename_dialog(pfc::string8* text, HWND parent)
{
    rename_param param;
    param.m_text = text;
    return !!uDialogBox(IDD_RENAME_PLAYLIST, parent, RenameProc, (LPARAM)(&param));
}

void rename_playlist(size_t index, HWND wnd_parent)
{
    static_api_ptr_t<playlist_manager> playlist_api;
    pfc::string8 temp;
    if (playlist_api->playlist_get_name(index, temp)) {
        if (rename_dialog(&temp, wnd_parent)) { // fucko: dialogobx has a messgeloop, someone might have called
                                                // switcher api funcs in the meanwhile
            size_t num = playlist_api->get_playlist_count();
            if (index < num) {
                playlist_api->playlist_rename(index, temp, temp.length());
            }
        }
    }
}

bool check_clipboard()
{
    static_api_ptr_t<ole_interaction> api;
    pfc::com_ptr_t<IDataObject> pDO;

    HRESULT hr = OleGetClipboard(pDO.receive_ptr());
    if (FAILED(hr))
        return false;

    hr = api->check_dataobject_playlists(pDO);
    return SUCCEEDED(hr);
}
bool cut(const pfc::bit_array& mask)
{
    static_api_ptr_t<playlist_manager> m_playlist_api;
    static_api_ptr_t<ole_interaction> api;
    playlist_dataobject_desc_impl data;

    data.set_from_playlist_manager(mask);
    pfc::com_ptr_t<IDataObject> pDO = api->create_dataobject(data);

    HRESULT hr = OleSetClipboard(pDO.get_ptr());
    if (FAILED(hr))
        return false;

    m_playlist_api->remove_playlists(mask);
    if (m_playlist_api->get_active_playlist() == pfc_infinite && m_playlist_api->get_playlist_count())
        m_playlist_api->set_active_playlist(0);

    return true;
}
bool cut(const pfc::list_base_const_t<t_size>& indices)
{
    static_api_ptr_t<playlist_manager> m_playlist_api;
    t_size count = indices.get_count(), playlist_count = m_playlist_api->get_playlist_count();
    pfc::bit_array_bittable mask(playlist_count);
    for (t_size i = 0; i < count; i++) {
        if (indices[i] < playlist_count)
            mask.set(indices[i], true);
    }
    return cut(mask);
};
bool copy(const pfc::bit_array& mask)
{
    static_api_ptr_t<playlist_manager> m_playlist_api;
    static_api_ptr_t<ole_interaction> api;
    playlist_dataobject_desc_impl data;

    data.set_from_playlist_manager(mask);
    pfc::com_ptr_t<IDataObject> pDO = api->create_dataobject(data);

    HRESULT hr = OleSetClipboard(pDO.get_ptr());
    return SUCCEEDED(hr);
}
bool copy(const pfc::list_base_const_t<t_size>& indices)
{
    static_api_ptr_t<playlist_manager> m_playlist_api;
    t_size count = indices.get_count(), playlist_count = m_playlist_api->get_playlist_count();
    pfc::bit_array_bittable mask(playlist_count);
    for (t_size i = 0; i < count; i++) {
        if (indices[i] < playlist_count)
            mask.set(indices[i], true);
    }
    return copy(mask);
};
bool paste(HWND wnd, t_size index_insert)
{
    static_api_ptr_t<playlist_manager> m_playlist_api;
    static_api_ptr_t<ole_interaction> api;
    pfc::com_ptr_t<IDataObject> pDO;

    HRESULT hr = OleGetClipboard(pDO.receive_ptr());
    if (FAILED(hr))
        return false;

    playlist_dataobject_desc_impl data;
    hr = api->check_dataobject_playlists(pDO);
    if (FAILED(hr))
        return false;

    hr = api->parse_dataobject_playlists(pDO, data);
    if (FAILED(hr))
        return false;

    t_size plcount = m_playlist_api->get_playlist_count();
    if (index_insert > plcount)
        index_insert = plcount;

    t_size count = data.get_entry_count();
    for (t_size i = 0; i < count; i++) {
        pfc::string8 name;
        metadb_handle_list handles;
        data.get_entry_name(i, name);
        data.get_entry_content(i, handles);
        index_insert = m_playlist_api->create_playlist(name, pfc_infinite, index_insert);
        m_playlist_api->playlist_insert_items(index_insert, 0, handles, pfc::bit_array_false());
        index_insert++;
    }
    return true;
};
} // namespace playlist_manager_utils
