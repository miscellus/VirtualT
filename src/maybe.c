/*
==========================================================================
Window handler for all events. (SDL3 Refactor)
==========================================================================
*/
char keylabel[128];

// Assuming VT_SIM_KEYUP / VT_SIM_KEYDOWN are handled either via custom SDL_Event types 
// or stripped out to a separate function. For this example, we'll assume they 
// are pushed as custom SDL events (SDL_EVENT_USER + X).

int T100_Disp::handle(const SDL_Event *event)
{
	char keystr[10];
	char isSpecialKey = 1;
	int  c;
	int  simulated = FALSE;
	
	// SDL3 handles simulated keys differently, often pushed via SDL_PushEvent.
	// We'll flag it if you set a custom event type or flag.
	if (event->type >= SDL_EVENT_USER) 
	{
		simulated = TRUE;
		// Map your custom simulation logic here if needed
	}

	switch (event->type)
	{
	case SDL_EVENT_MOUSE_WHEEL:
	{
		float x = event->wheel.x;
		float y = event->wheel.y;

		// Test for up/down arrow
		if (((gSpecialKeys & (MT_GRAPH | MT_CODE)) != (MT_GRAPH | MT_CODE)) || IsInMenu())
		{
			if (y < 0)
				WheelKey(SDLK_LEFT);
			else if (y > 0)
				WheelKey(SDLK_RIGHT);
		}
		else
		{
			if (y < 0)
				WheelKey(SDLK_UP);
			else if (y > 0)
				WheelKey(SDLK_DOWN);
		}
		if (x < 0)
			WheelKey(SDLK_LEFT);
		else if (x > 0)
			WheelKey(SDLK_RIGHT);
		return 1;
	}

	case SDL_EVENT_DROP_FILE:
	case SDL_EVENT_DROP_BEGIN:
	case SDL_EVENT_DROP_COMPLETE:
		return 1;

	case SDL_EVENT_CLIPBOARD_UPDATE:
		if (IsInMenu() == 1)
		{
			char* text = SDL_GetClipboardText();
			if (text) {
				remote_load_from_host(text);
				SDL_free(text);
			}
		}
		return 1;

	case SDL_EVENT_WINDOW_FOCUS_GAINED:
		m_MyFocus = 1;
		break;

	case SDL_EVENT_WINDOW_MOUSE_ENTER:
	case SDL_EVENT_WINDOW_MOUSE_LEAVE:
		return 1;

	case SDL_EVENT_MOUSE_MOTION:
		if (m_HaveMouse)
		{
			int mx = (int)event->motion.x;
			int my = (int)event->motion.y;

			if (IsInText())
				MouseMoveInText(mx, my);
		}
		return 1;

	case SDL_EVENT_MOUSE_BUTTON_UP:
		if (m_Select)
		{
			if (m_WheelKeyIn == m_WheelKeyOut)
			{
				// Note: Fl::add_timeout needs replacing with SDL_AddTimer
				// SDL_AddTimer(20, cb_await_selection_complete, NULL);
			}
		}

		if (event->button.clicks == 1) // roughly equivalent to Fl::event_clicks() == 0 check
			m_Select = FALSE;
		m_HaveMouse = FALSE;
		// Fl::release() - Not needed in SDL, mouse grab ends naturally or via SDL_SetWindowMouseGrab
		break;

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	{
		// Fl::grab() equivalent if strict grabbing is needed:
		// SDL_SetWindowMouseGrab(window, SDL_TRUE); 
		m_HaveMouse = TRUE;

		if (event->button.button == SDL_BUTTON_RIGHT) // FL_BUTTON3
		{
			if (gModel == MODEL_M100 && gQuad)
			{
				// FLTK popup logic needs to be replaced with your new GUI (e.g. ImGui or native SDL menus)
			}
			take_focus();
			return 1;
		}

		if (m_MyFocus == 1)
		{
			int mx = (int)event->button.x;
			int my = (int)event->button.y;
			int procFkey = FALSE;
			int whichMenu;

			if ((mx >= m_BezelLeft  + m_BezelLeftW) && (mx <= m_BezelRight) &&
				(my >= m_BezelTop + m_BezelTopH) && (my <= m_BezelBottom))
			{
				if (event->button.clicks > 1) // Double click
				{
					if (IsInMenu())
					{
						WheelKey(SDLK_RETURN);
						m_Select = FALSE;
						return 1;
					}
				}
				
				if ((whichMenu = IsInMenu()) != 0)
				{
					m_Select = FALSE;
					if (my >= m_BezelBottom - 1 - 8 * MultFact)
						procFkey = TRUE;
					else
					{
						if (whichMenu == 1)
							return ButtonClickInMenu(mx, my);
					}
				}
				else
				{
					if (gInMsPlanROM)
						return ButtonClickInMsPlan(mx, my);
					int labelEn = get_memory8(gStdRomDesc->sLabelEn);
					if ((gStdRomDesc->sLabelEn && labelEn) &&
						(my >= m_BezelBottom - 1 - 8 * MultFact))
					{
							procFkey = TRUE;
					}
					else if (IsInText())
					{
						return ButtonClickInText(mx, my);
					}
				}
			}
			else if ((mx >= gXoffset) && (mx <= m_BezelRight) && 
				(my >= m_BezelBottom) && (my <= m_BezelBottom + m_BezelBottomH))
			{
				procFkey = TRUE;
			}

			if (procFkey)
			{
				int num_labels = gModel == MODEL_PC8201 ? 5 : 8;
				int pixPerLabel = 240 / num_labels;
				int fk = (mx - gXoffset) / MultFact / pixPerLabel + 1;
				WheelKey(SDLK_F1 + fk - 1); 
				m_Select = FALSE;
			}
		}
		m_MyFocus = 1;
		break;
	}

	case SDL_EVENT_WINDOW_FOCUS_LOST:
		m_MyFocus = 0;
		gSpecialKeys = 0xFFFFFFFF;
		// GUI Deactivations go here
		for (c = 0; c < 128; c++)
			gKeyStates[c] = 0;
		update_keys();
		break;

	case SDL_EVENT_KEY_UP:
	case SDL_EVENT_KEY_DOWN:
	{
		if (!m_MyFocus && !simulated)
			return 1;

		bool isKeyDown = (event->type == SDL_EVENT_KEY_DOWN);
		SDL_Keycode key = event->key.key;
		
		// Helper lambda to check shift state easily
		auto hasShift = []() { return (SDL_GetModState() & SDL_KMOD_SHIFT) != 0; };
		auto hasCtrl  = []() { return (SDL_GetModState() & SDL_KMOD_CTRL) != 0; };

		if (isKeyDown && (ioBA & 0x10))
		{
			resetcpu();
			break;
		}

		// Handle Special Modifiers toggling
		if (isKeyDown)
		{
			switch (key)
			{
			case SDLK_ESCAPE: gSpecialKeys &= ~MT_ESC; break;
			case SDLK_DELETE: gSpecialKeys &= ~MT_SHIFT; 
			case SDLK_BACKSPACE: gSpecialKeys &= ~MT_BKSP; break;
			case SDLK_TAB: gSpecialKeys &= ~MT_TAB; break;
			case SDLK_RETURN:
			case SDLK_KP_ENTER: gSpecialKeys &= ~MT_ENTER; break;
			case SDLK_PRINTSCREEN: gSpecialKeys &= ~MT_PRINT; break;
			case SDLK_F12:
			case SDLK_PAUSE: gSpecialKeys &= ~MT_PAUSE; break;
			case SDLK_LEFT: gSpecialKeys &= ~MT_LEFT; break;
			case SDLK_UP: gSpecialKeys &= ~MT_UP; break;
			case SDLK_RIGHT: gSpecialKeys &= ~MT_RIGHT; break;
			case SDLK_DOWN: gSpecialKeys &= ~MT_DOWN; break;
			case SDLK_LSHIFT:
			case SDLK_RSHIFT: gSpecialKeys &= ~MT_SHIFT; break;
			case SDLK_LCTRL:
			case SDLK_RCTRL: gSpecialKeys &= ~MT_CTRL; break;
			case SDLK_CAPSLOCK:
				gSpecialKeys ^= MT_CAP_LOCK;
				// UI toggle logic here
				break;
			case SDLK_LALT: gSpecialKeys &= ~MT_GRAPH; break;
			case SDLK_RALT: gSpecialKeys &= ~MT_CODE; break;
			case SDLK_INSERT:
			case SDLK_HOME: gSpecialKeys &= ~(MT_CTRL | MT_LEFT); break;
			case SDLK_END: gSpecialKeys &= ~(MT_CTRL | MT_RIGHT); break;
			case SDLK_PAGEUP: gSpecialKeys &= ~(MT_SHIFT | MT_UP); break;
			case SDLK_PAGEDOWN: gSpecialKeys &= ~(MT_SHIFT | MT_DOWN); break;
			case SDLK_F1: gSpecialKeys &= ~MT_F1; break;
			case SDLK_F2: gSpecialKeys &= ~MT_F2; break;
			case SDLK_F3: gSpecialKeys &= ~MT_F3; break;
			case SDLK_F4: gSpecialKeys &= ~MT_F4; break;
			case SDLK_F5: gSpecialKeys &= ~MT_F5; break;
			case SDLK_F6: gSpecialKeys &= ~MT_F6; break;
			case SDLK_F7: gSpecialKeys &= ~MT_F7; break;
			case SDLK_F8: gSpecialKeys &= ~MT_F8; break;
			case SDLK_F9: gSpecialKeys &= ~MT_LABEL; break;
			case SDLK_F10: gSpecialKeys &= ~MT_PRINT; break;
			case SDLK_F11: gSpecialKeys &= ~MT_PASTE; break;
			case SDLK_NUMLOCKCLEAR: gSpecialKeys &= ~MT_NUM; break;
			default:
				// Map printable ascii
				if (key < 128)
				{
					gKeyStates[key] = 1;
					if (key == SDLK_SPACE)
						gSpecialKeys &= ~MT_SPACE;

					// --- Insert your PC8201 / M10 special logic here ---
					// Example:
					if (gModel == MODEL_PC8201 || gModel == MODEL_PC8300)
					{
						if (key == '6') {
							if (hasShift()) { gKeyStates['6'] = 0; gKeyStates['@'] = 1; }
							else { gSpecialKeys |= MT_SHIFT; gKeyStates['@'] = 0; }
						}
						// ... map the rest of your custom layout logic identically using hasShift() ...
					}
				}
				isSpecialKey = 0;
				break;
			}
		}
		else // KEY_UP
		{
			switch (key)
			{
			case SDLK_ESCAPE: gSpecialKeys |= MT_ESC; break;
			case SDLK_DELETE: if (!hasShift()) gSpecialKeys |= MT_SHIFT;
			case SDLK_BACKSPACE: gSpecialKeys |= MT_BKSP; break;
			case SDLK_TAB: gSpecialKeys |= MT_TAB; break;
			case SDLK_RETURN:
			case SDLK_KP_ENTER: gSpecialKeys |= MT_ENTER; break;
			case SDLK_PRINTSCREEN: gSpecialKeys |= MT_PRINT; break;
			case SDLK_F12:
			case SDLK_PAUSE: gSpecialKeys |= MT_PAUSE; break;
			case SDLK_LEFT: gSpecialKeys |= MT_LEFT; break;
			case SDLK_UP: gSpecialKeys |= MT_UP; break;
			case SDLK_RIGHT: gSpecialKeys |= MT_RIGHT; break;
			case SDLK_DOWN: gSpecialKeys |= MT_DOWN; break;
			case SDLK_LSHIFT:
			case SDLK_RSHIFT: gSpecialKeys |= MT_SHIFT; break;
			case SDLK_LCTRL:
			case SDLK_RCTRL: gSpecialKeys |= MT_CTRL; break;
			case SDLK_LALT: gSpecialKeys |= MT_GRAPH; break;
			case SDLK_RALT: gSpecialKeys |= MT_CODE; break;
			case SDLK_INSERT:
			case SDLK_HOME:
				if (!hasCtrl()) gSpecialKeys |= MT_CTRL;
				if (!(SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LEFT])) gSpecialKeys |= MT_LEFT;
				break;
			// ... continue F1-F11 mappings ...
			default:
				if (key < 128)
				{
					gKeyStates[key] = 0;
					if (key == SDLK_SPACE)
						gSpecialKeys |= MT_SPACE;
						
					// Your custom keyup logic...
				}
				isSpecialKey = 0;
				break;
			}
		}
		
		update_keys();
		break;
	}
	default:
		break; // Handle other SDL events if necessary
	}

	// Display keystroke info on status line of display
	if (event->type == SDL_EVENT_KEY_UP || event->type == SDL_EVENT_KEY_DOWN)
	{
		keylabel[0] = 0;

		for (c = 0; c < 32; c++)
		{
			if ((gSpecialKeys & (0x01 << c)) == 0)
			{
				if ((c == 2) || (c == 3) || (c == 5)) continue;
				if (keylabel[0] != 0) strcat(keylabel, "+");
				strcat(keylabel, gSpKeyText[c]);
			}
		}

		for (c = ' ' + 1; c < '~'; c++)
		{
			if (gKeyStates[c])
			{
				if (keylabel[0] != 0) strcat(keylabel, "+");
				sprintf(keystr, "'%c'", c);

				if (c == 0x20) strcat(keylabel, "SPACE");
				else strcat(keylabel, keystr);
			}
		}
		
		// Update your GUI with the new text
		// gpKey->label(keylabel);
	}

	return 1;
}