#include "stdafx.h"
#include "common.h"

const char* strchr_n(const char* src, char c, unsigned len)
{
    const char* ptr = src;
    const char* start = ptr;
    while (*ptr && ((unsigned)(ptr - start) < len)) {
        if (*ptr == c)
            return ptr;
        ptr++;
    }
    return nullptr;
}

void colour::set(COLORREF new_colour)
{
    B = LOBYTE(HIWORD(new_colour));
    G = HIBYTE(LOWORD(new_colour));
    R = LOBYTE(LOWORD(new_colour));
}

void set_sel_single(int idx, bool toggle, bool focus, bool single_only)
{
    static_api_ptr_t<playlist_manager> playlist_api;
    unsigned total = playlist_api->activeplaylist_get_item_count();
    unsigned idx_focus = playlist_api->activeplaylist_get_focus_item();

    pfc::bit_array_bittable mask(total);
    mask.set(idx, toggle ? !playlist_api->activeplaylist_is_item_selected(idx) : true);

    if (single_only) {
        playlist_api->activeplaylist_set_selection(pfc::bit_array_true(), mask);
    } else if (toggle || !playlist_api->activeplaylist_is_item_selected(idx)) {
        playlist_api->activeplaylist_set_selection(pfc::bit_array_one(idx), mask);
    }

    if (focus && idx_focus != idx)
        playlist_api->activeplaylist_set_focus_item(idx);
}

void set_sel_range(int start, int end, bool keep, bool deselect)
{
    //    console::info(pfc::string_printf("%i %i %i %i",start,end,(long)keep,(long)deselect));
    static_api_ptr_t<playlist_manager> playlist_api;
    unsigned total = playlist_api->activeplaylist_get_item_count();
    pfc::bit_array_bittable mask(total);
    //    if (keep)
    //        playlist_api->activeplaylist_get_selection_mask(mask);
    int n = start, t = end;

    if (n < t)
        for (; n <= t; n++)
            mask.set(n, !deselect);
    else if (n >= t)
        for (; n >= t; n--)
            mask.set(n, !deselect);

    int from = start < end ? start + 1 : end;
    int to = (start < end ? end + 1 : start);

    if (keep)
        playlist_api->activeplaylist_set_selection(pfc::bit_array_range(from, to - from, true), mask);
    else
        playlist_api->activeplaylist_set_selection(pfc::bit_array_true(), mask);
}

UINT GetNumScrollLines()
{
    UINT ucNumLines = 3; // 3 is the default
    SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &ucNumLines, 0);
    return (ucNumLines);
}

string_pn::string_pn(metadb_handle_list_cref handles, const char* format, const char* def)
{
    pfc::string8_fast_aggressive a, b;
    a.prealloc(512);
    b.prealloc(512);
    unsigned count = handles.get_count();
    bool use = false;

    pfc::ptr_list_t<char> specs;

    const char* ptr = format;
    while (*ptr) {
        const char* start = ptr;
        while (*ptr && *ptr != '\\')
            ptr++;
        if (ptr > start)
            specs.add_item(pfc::strdup_n(start, ptr - start));
        while (*ptr == '\\')
            ptr++;
    }

    unsigned fmt_count = specs.get_count();

    for (unsigned f = 0; f < fmt_count; f++) {
        service_ptr_t<titleformat_object> to_temp;
        static_api_ptr_t<titleformat_compiler>()->compile_safe(to_temp, specs[f]);
        for (unsigned n = 0; n < count; n++) {
            if (n == 0) {
                handles[0]->format_title(nullptr, a, to_temp, nullptr);
                use = true;
            } else {
                handles[n]->format_title(nullptr, b, to_temp, nullptr);
                if (strcmp(a, b) != 0) {
                    use = false;
                    break;
                }
            }
        }

        if (use)
            break;
    }

    specs.free_all();

    if (use)
        set_string(a);
    else
        set_string(def);
}

void g_save_playlist(HWND wnd, const pfc::list_base_const_t<metadb_handle_ptr>& p_items, const char* p_name)
{
    pfc::string8 name = p_name;
    // name << p_name;

    pfc::string_formatter ext;
    service_enum_t<playlist_loader> e;
    service_ptr_t<playlist_loader> ptr;
    unsigned def_index = 0, n = 0;

    while (e.next(ptr)) {
        if (ptr->can_write()) {
            ext << ptr->get_extension() << " files|*." << ptr->get_extension() << "|";
            if (!stricmp_utf8(ptr->get_extension(), "fpl"))
                def_index = n;
            n++;
        }
    }
    if (uGetOpenFileName(wnd, ext, def_index, "fpl", "Save playlist...", nullptr, name, TRUE)) {
        try {
            abort_callback_dummy p_abort;
            playlist_loader::g_save_playlist(name, p_items, p_abort);
        } catch (pfc::exception& e) {
            popup_message::g_show(e.what(), "Error writing playlist", popup_message::icon_error);
        }
    }
}
