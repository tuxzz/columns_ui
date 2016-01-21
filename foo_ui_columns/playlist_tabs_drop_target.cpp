#include "stdafx.h"


HRESULT STDMETHODCALLTYPE playlists_tabs_extension::playlists_tabs_drop_target::QueryInterface(REFIID riid, LPVOID FAR *ppvObject)
{
	if (ppvObject == NULL) return E_INVALIDARG;
	*ppvObject = NULL;
	if (riid == IID_IUnknown) { AddRef(); *ppvObject = (IUnknown*)this; return S_OK; }
	else if (riid == IID_IDropTarget) { AddRef(); *ppvObject = (IDropTarget*)this; return S_OK; }
	else return E_NOINTERFACE;
}
ULONG STDMETHODCALLTYPE   playlists_tabs_extension::playlists_tabs_drop_target::AddRef()
{
	return InterlockedIncrement(&drop_ref_count);
}
ULONG STDMETHODCALLTYPE   playlists_tabs_extension::playlists_tabs_drop_target::Release()
{
	LONG rv = InterlockedDecrement(&drop_ref_count);
	if (!rv)
	{
#ifdef _DEBUG
		OutputDebugString(_T("deleting playlists_tabs_extension"));
#endif
		delete this;
	}
	return rv;
}

HRESULT STDMETHODCALLTYPE playlists_tabs_extension::playlists_tabs_drop_target::DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL ptl, DWORD *pdwEffect)
{
	POINT pt = { ptl.x, ptl.y };
	if (m_DropTargetHelper.is_valid()) m_DropTargetHelper->DragEnter(p_list->get_wnd(), pDataObj, &pt, *pdwEffect);
	m_DataObject = pDataObj;

	last_over.x = 0;
	last_over.y = 0;
	m_last_rmb = ((grfKeyState & MK_RBUTTON) != 0);

	if (ui_drop_item_callback::g_is_accepted_type(pDataObj, pdwEffect))
	{
		return S_OK;
	}
	else if (static_api_ptr_t<playlist_incoming_item_filter>()->process_dropped_files_check(pDataObj))
	{
		*pdwEffect = DROPEFFECT_COPY;
		return S_OK;
	}
	return S_FALSE;
}


HRESULT STDMETHODCALLTYPE playlists_tabs_extension::playlists_tabs_drop_target::DragOver(DWORD grfKeyState, POINTL ptl, DWORD *pdwEffect)
{
	POINT pt = { ptl.x, ptl.y };
	if (m_DropTargetHelper.is_valid()) m_DropTargetHelper->DragOver(&pt, *pdwEffect);

	*pdwEffect = DROPEFFECT_COPY;
	m_last_rmb = ((grfKeyState & MK_RBUTTON) != 0);

	if (last_over.x != pt.x || last_over.y != pt.y)
	{

		static_api_ptr_t<playlist_manager> playlist_api;
		POINT pti, ptm;
		pti.y = pt.y;
		pti.x = pt.x;

		ptm = pti;
		ScreenToClient(p_list->get_wnd(), &ptm);

		HWND wnd = ChildWindowFromPointEx(p_list->get_wnd(), ptm, CWP_SKIPINVISIBLE);

		//	RECT plist;

		//	GetWindowRect(g_plist, &plist);
		//	RECT tabs;

		//	GetWindowRect(g_tab, &tabs);

		if (p_list->wnd_tabs)
		{

			POINT pttab;
			pttab = pti;


			if (wnd == p_list->wnd_tabs && ScreenToClient(p_list->wnd_tabs, &pttab))
			{
				TCHITTESTINFO hittest;
				hittest.pt.x = pttab.x;
				hittest.pt.y = pttab.y;
				int idx = TabCtrl_HitTest(p_list->wnd_tabs, &hittest);
				int old = playlist_api->get_active_playlist();
				if (cfg_drag_autoswitch && idx >= 0 && old != idx) p_list->switch_to_playlist_delayed2(idx);//playlist_switcher::get()->set_active_playlist(idx);
				else p_list->kill_switch_timer();

				if (idx != -1)
				{
					pfc::string8 name;
					static_api_ptr_t<playlist_manager>()->playlist_get_name(idx, name);
					mmh::ole::SetDropDescription(m_DataObject.get_ptr(), DROPIMAGE_COPY, "Add to %1", name);
				}
				else
					mmh::ole::SetDropDescription(m_DataObject.get_ptr(), DROPIMAGE_COPY, "Add to new playlist", "");
			}
			else
				mmh::ole::SetDropDescription(m_DataObject.get_ptr(), DROPIMAGE_COPY, "Add to new playlist", "");

		}
		if ((!p_list->wnd_tabs || wnd != p_list->wnd_tabs))  p_list->kill_switch_timer();

		last_over = ptl;
	}

	return S_OK;



}

HRESULT STDMETHODCALLTYPE playlists_tabs_extension::playlists_tabs_drop_target::DragLeave(void)
{
	if (m_DropTargetHelper.is_valid()) m_DropTargetHelper->DragLeave();
	last_over.x = 0;
	last_over.y = 0;
	p_list->kill_switch_timer();
	mmh::ole::SetDropDescription(m_DataObject.get_ptr(), DROPIMAGE_INVALID, "", "");
	m_DataObject.release();

	return S_OK;
}

HRESULT STDMETHODCALLTYPE playlists_tabs_extension::playlists_tabs_drop_target::Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL ptl, DWORD *pdwEffect)
{
	POINT pt = { ptl.x, ptl.y };
	if (m_DropTargetHelper.is_valid()) m_DropTargetHelper->Drop(pDataObj, &pt, *pdwEffect);

	p_list->kill_switch_timer();

	static_api_ptr_t<playlist_manager> playlist_api;

	POINT pti, ptm;
	pti.y = pt.y;
	pti.x = pt.x;

	ptm = pti;
	ScreenToClient(p_list->get_wnd(), &ptm);

	HWND wnd = ChildWindowFromPointEx(p_list->get_wnd(), ptm, CWP_SKIPINVISIBLE);

	if (wnd)
	{

		bool process = !ui_drop_item_callback::g_on_drop(pDataObj);

		bool send_new_playlist = false;

		if (process && m_last_rmb)
		{
			process = false;
			enum { ID_DROP = 1, ID_NEW_PLAYLIST, ID_CANCEL };

			HMENU menu = CreatePopupMenu();

			uAppendMenu(menu, (MF_STRING), ID_DROP, "&Add files here");
			uAppendMenu(menu, (MF_STRING), ID_NEW_PLAYLIST, "&Add files to new playlist");
			uAppendMenu(menu, MF_SEPARATOR, 0, 0);
			uAppendMenu(menu, MF_STRING, ID_CANCEL, "&Cancel");

			int cmd = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, pt.x, pt.y, 0, p_list->get_wnd(), 0);
			DestroyMenu(menu);

			if (cmd)
			{
				switch (cmd)
				{
				case ID_DROP:
					process = true;
					break;
				case ID_NEW_PLAYLIST:
					process = true;
					send_new_playlist = true;
					break;
				}
			}
		}

		if (process)
		{
			metadb_handle_list data;
			static_api_ptr_t<playlist_incoming_item_filter> incoming_api;
			incoming_api->process_dropped_files(pDataObj, data, true, p_list->get_wnd());

			POINT pttab, ptpl;
			pttab = pti;
			ptpl = pti;


			int idx = -1;
			//	if ((g_tab && wnd == g_tab) || g_plist && wnd == g_plist)


			//	bool processed = false;
			t_size target_index = playlist_api->get_active_playlist();


			if (p_list->wnd_tabs && wnd == p_list->wnd_tabs && !send_new_playlist)
			{
				RECT tabs;

				GetWindowRect(p_list->wnd_tabs, &tabs);
				if (ScreenToClient(p_list->wnd_tabs, &pttab))
				{
					TCHITTESTINFO hittest;
					hittest.pt.x = pttab.x;
					hittest.pt.y = pttab.y;
					int idx = TabCtrl_HitTest(p_list->wnd_tabs, &hittest);
					int old = playlist_api->get_active_playlist();
					if (idx < 0)
					{
						send_new_playlist = true;
					}
					else target_index = idx;
				}

			}


			if (send_new_playlist)
			{
				pfc::string8 playlist_name("Untitled");

				bool named = false;

				if (1 || 1)
				{
					FORMATETC   fe;
					STGMEDIUM   sm;
					HRESULT     hr = E_FAIL;

					//					memset(&sm, 0, sizeof(0));

					fe.cfFormat = CF_HDROP;
					fe.ptd = NULL;
					fe.dwAspect = DVASPECT_CONTENT;
					fe.lindex = -1;
					fe.tymed = TYMED_HGLOBAL;

					// User has dropped on us. Get the data from drag source
					hr = pDataObj->GetData(&fe, &sm);
					if (SUCCEEDED(hr))
					{

						// Display the data and release it.
						pfc::string8 temp;

						unsigned int /*n,*/t = uDragQueryFileCount((HDROP)sm.hGlobal);
						if (t == 1)
						{
							{
								uDragQueryFile((HDROP)sm.hGlobal, 0, temp);
								if (uGetFileAttributes(temp) & FILE_ATTRIBUTE_DIRECTORY)
								{
									playlist_name.set_string(pfc::string_filename_ext(temp));
									named = true;
								}
								else
								{
									playlist_name.set_string(pfc::string_filename(temp));
									named = true;
#if 0
									pfc::string_extension ext(temp);

									service_enum_t<playlist_loader> e;
									service_ptr_t<playlist_loader> l;
									if (e.first(l))
										do
										{
											if (!strcmp(l->get_extension(), ext))
											{
												playlist_name.set_string(pfc::string_filename(temp));
												named = true;
												l.release();
												break;
											}
											l.release();
										} while (e.next(l));
#endif
								}

							}
						}

						ReleaseStgMedium(&sm);
					}
				}

				unsigned new_idx;

				if (named && cfg_replace_drop_underscores) playlist_name.replace_char('_', ' ', 0);
				if (!named && cfg_pgen_tf) new_idx = playlist_api->create_playlist(string_pn(data, cfg_pgenstring), pfc_infinite, playlist_api->get_playlist_count());

				else  new_idx = playlist_api->create_playlist(playlist_name, pfc_infinite, playlist_api->get_playlist_count());

				playlist_api->playlist_add_items(new_idx, data, bit_array_false());
				if (main_window::config_get_activate_target_playlist_on_dropped_items())
					playlist_api->set_active_playlist(new_idx);

			}
			else
			{
				playlist_api->playlist_clear_selection(target_index);
				playlist_api->playlist_insert_items(target_index, idx, data, bit_array_true());
				if (main_window::config_get_activate_target_playlist_on_dropped_items())
					playlist_api->set_active_playlist(target_index);
			}

			data.remove_all();
		}
	}
	m_DataObject.release();

	return S_OK;
}
playlists_tabs_extension::playlists_tabs_drop_target::playlists_tabs_drop_target(playlists_tabs_extension * p_wnd) : drop_ref_count(0), p_list(p_wnd), m_last_rmb(false)
{
	last_over.x = 0; last_over.y = 0;
	m_DropTargetHelper.instantiate(CLSID_DragDropHelper, NULL, CLSCTX_INPROC);
}