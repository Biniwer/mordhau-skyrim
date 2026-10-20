#include "CharacterRenderInternal.h"

namespace dm::CharacterRender
{
	inline namespace internal
	{
		void HudCaptureLoop()
		{
			std::vector<std::uint8_t> full;
			for (;;) {
				Sleep(40);
				if (!Link::Current().live) {
					continue;
				}
				HWND window = nullptr, found = nullptr;
				while ((window = FindWindowExW(nullptr, window, L"UnrealWindow", nullptr)) != nullptr) {
					wchar_t title[64]{};
					GetWindowTextW(window, title, 63);
					if (std::wstring_view(title).find(L"MORDHAU") != std::wstring_view::npos) {
						found = window;
						break;
					}
				}
				RECT rc{};
				if (!found || IsIconic(found) || !GetClientRect(found, &rc) || rc.right < 320 || rc.bottom < 200) {
					continue;
				}
				const int w = rc.right, h = rc.bottom;
				HDC       windowDc = GetDC(found);
				HDC       memoryDc = CreateCompatibleDC(windowDc);
				HBITMAP   bitmap = CreateCompatibleBitmap(windowDc, w, h);
				HGDIOBJ   old = SelectObject(memoryDc, bitmap);
				if (PrintWindow(found, memoryDc, 3)) {  // client area only, rendered content (works while covered by Skyrim)
					BITMAPINFO info{};
					info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
					info.bmiHeader.biWidth = w;
					info.bmiHeader.biHeight = -h;
					info.bmiHeader.biPlanes = 1;
					info.bmiHeader.biBitCount = 32;
					full.resize(static_cast<std::size_t>(w) * h * 4);
					if (GetDIBits(memoryDc, bitmap, 0, static_cast<UINT>(h), full.data(), &info, DIB_RGB_COLORS)) {
						// the picture MORDHAU draws can be a little smaller than the window's client area; what is left over
						// at the right and bottom comes back pure white. Measure it, so "the centre" is MORDHAU's centre.
						const auto white = [&](int x, int y) {
							const std::uint8_t* px = &full[(static_cast<std::size_t>(y) * w + x) * 4];
							return px[0] == 255 && px[1] == 255 && px[2] == 255;
						};
						int pictureW = w, pictureH = h;
						while (pictureH > 200 && white(8, pictureH - 1)) {
							--pictureH;
						}
						while (pictureW > 320 && white(pictureW - 1, 8)) {
							--pictureW;
						}
						std::scoped_lock lock(g_hud.lock);
						for (int k = 0; k < kHudPartCount; ++k) {
							const auto& part = kHudParts[k];
							const int   x0 = static_cast<int>(pictureW * part.left), x1 = static_cast<int>(pictureW * part.right), y0 = static_cast<int>(pictureH * part.top), y1 = static_cast<int>(pictureH * part.bottom);
							const int   cw = x1 - x0, ch = y1 - y0;
							g_hud.pixels[k].resize(static_cast<std::size_t>(cw) * ch * 4);
							// (MORDHAU's health / stamina bars are not shown - sheets/builds.json mordhau_bars: their pixels are
							// not copied. The part keeps its size, which is what tells a fresh picture from none.)
							for (int y = 0; y < ch && (k != 0 || g_mordhauBars.load()); ++y) {
								std::memcpy(&g_hud.pixels[k][static_cast<std::size_t>(y) * cw * 4], &full[(static_cast<std::size_t>(y0 + y) * w + x0) * 4], static_cast<std::size_t>(cw) * 4);
							}
							g_hud.width[k] = cw;
							g_hud.height[k] = ch;
						}
						++g_hud.version;
						g_hud.when = std::chrono::steady_clock::now();
					}
				}
				SelectObject(memoryDc, old);
				DeleteObject(bitmap);
				DeleteDC(memoryDc);
				ReleaseDC(found, windowDc);
			}
		}

		// Brings the captured parts onto textures. False when there is no fresh picture.
		bool UpdateHudTexture(ID3D11Device* a_device, ID3D11DeviceContext* a_context)
		{
			if (!g_hud.started) {
				g_hud.started = true;
				std::thread(HudCaptureLoop).detach();
				logger::info("character: reading MORDHAU's own HUD (crosshair with the attack-direction indicator, voice / emote menu) from its window");
			}
			std::scoped_lock lock(g_hud.lock);
			if (g_hud.width[0] <= 0 || std::chrono::duration<float>(std::chrono::steady_clock::now() - g_hud.when).count() > 1.5f) {
				return false;
			}
			for (int k = 0; k < kHudPartCount; ++k) {
				if (!g_hudTex[k] || g_hudTexW[k] != g_hud.width[k] || g_hudTexH[k] != g_hud.height[k]) {
					if (g_hudSrv[k]) {
						g_hudSrv[k]->Release();
						g_hudSrv[k] = nullptr;
					}
					if (g_hudTex[k]) {
						g_hudTex[k]->Release();
						g_hudTex[k] = nullptr;
					}
					D3D11_TEXTURE2D_DESC td{};
					td.Width = static_cast<UINT>(g_hud.width[k]);
					td.Height = static_cast<UINT>(g_hud.height[k]);
					td.MipLevels = td.ArraySize = 1;
					td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
					td.SampleDesc.Count = 1;
					td.Usage = D3D11_USAGE_DYNAMIC;
					td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
					td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
					if (FAILED(a_device->CreateTexture2D(&td, nullptr, &g_hudTex[k])) || FAILED(a_device->CreateShaderResourceView(g_hudTex[k], nullptr, &g_hudSrv[k]))) {
						return false;
					}
					g_hudTexW[k] = g_hud.width[k];
					g_hudTexH[k] = g_hud.height[k];
					g_hudTexVersion = 0;
				}
			}
			if (g_hudTexVersion != g_hud.version) {
				for (int k = 0; k < kHudPartCount; ++k) {
					D3D11_MAPPED_SUBRESOURCE mapped{};
					if (SUCCEEDED(a_context->Map(g_hudTex[k], 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
						for (int y = 0; y < g_hud.height[k]; ++y) {
							std::memcpy(static_cast<std::uint8_t*>(mapped.pData) + static_cast<std::size_t>(y) * mapped.RowPitch, &g_hud.pixels[k][static_cast<std::size_t>(y) * g_hud.width[k] * 4],
								static_cast<std::size_t>(g_hud.width[k]) * 4);
						}
						a_context->Unmap(g_hudTex[k], 0);
					}
				}
				g_hudTexVersion = g_hud.version;
			}
			return g_hudSrv[0] != nullptr && g_hudSrv[1] != nullptr;
		}

		void DrawHudOnBackBuffer(IDXGISwapChain* a_chain)
		{
			if (std::chrono::duration<float>(std::chrono::steady_clock::now() - g_hudWantedAt).count() > 0.25f) {
				return;  // not in play with the weapon out right now
			}
			auto* ui = RE::UI::GetSingleton();
			if (!ui || ui->GameIsPaused() || !g_presentDevice || !g_presentContext || !UpdateHudTexture(g_presentDevice, g_presentContext)) {
				return;
			}
			ID3D11Texture2D* back = nullptr;
			if (FAILED(a_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back))) || !back) {
				return;
			}
			D3D11_TEXTURE2D_DESC bd{};
			back->GetDesc(&bd);
			ID3D11RenderTargetView* rtv = nullptr;
			if (SUCCEEDED(g_presentDevice->CreateRenderTargetView(back, nullptr, &rtv)) && rtv) {
				auto*       context = g_presentContext;
				const float w = static_cast<float>(bd.Width), h = static_cast<float>(bd.Height);
				Vertex      quads[6 * kHudPartCount]{};
				for (int k = 0; k < kHudPartCount; ++k) {
					// each part keeps the place it has in MORDHAU's own window: the bars low at the centre (drawn a little
					// smaller, 0.8), the crosshair and direction indicator exactly at the centre, both keeping their shape
					const auto& part = kHudParts[k];
					const float shrink = k == 0 ? 0.8f : 1.0f;
					const float drawH = (part.bottom - part.top) * h * shrink;
					const float drawW = drawH * static_cast<float>(g_hudTexW[k]) / static_cast<float>(std::max(g_hudTexH[k], 1));
					const float centreY = k == 0 ? 0.925f * h : k == 1 ? 0.5f * h : (part.top + part.bottom) * 0.5f * h;
					// (the menu keeps its corner: its left edge where MORDHAU has it)
					const float x0 = k == 2 ? part.left * w : 0.5f * w - drawW * 0.5f, x1 = x0 + drawW, yTop = centreY - drawH * 0.5f, yBottom = yTop + drawH;
					const float cx0 = x0 / w * 2.0f - 1.0f, cx1 = x1 / w * 2.0f - 1.0f, cy0 = 1.0f - yTop / h * 2.0f, cy1 = 1.0f - yBottom / h * 2.0f;
					const float corners[6][4]{ { cx0, cy0, 0, 0 }, { cx1, cy0, 1, 0 }, { cx1, cy1, 1, 1 }, { cx0, cy0, 0, 0 }, { cx1, cy1, 1, 1 }, { cx0, cy1, 0, 1 } };
					for (int i = 0; i < 6; ++i) {
						quads[k * 6 + i].pos[0] = corners[i][0], quads[k * 6 + i].pos[1] = corners[i][1];
						quads[k * 6 + i].uv[0] = corners[i][2], quads[k * 6 + i].uv[1] = corners[i][3];
					}
				}
				D3D11_MAPPED_SUBRESOURCE map{};
				if (SUCCEEDED(context->Map(g_hudVb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
					std::memcpy(map.pData, quads, sizeof(quads));
					context->Unmap(g_hudVb, 0);
					SavedState saved;
					saved.Save(context);
					D3D11_VIEWPORT vp{ 0.0f, 0.0f, w, h, 0.0f, 1.0f };
					const UINT     stride = sizeof(Vertex), offset = 0;
					const FLOAT    factor[4]{ 0, 0, 0, 0 };
					context->OMSetRenderTargets(1, &rtv, nullptr);
					context->RSSetViewports(1, &vp);
					context->RSSetState(g_raster);
					context->OMSetBlendState(g_blendAlpha, factor, 0xFFFFFFFF);
					context->OMSetDepthStencilState(g_depthOff, 0);
					context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					context->IASetInputLayout(g_layout);
					context->IASetVertexBuffers(0, 1, &g_hudVb, &stride, &offset);
					context->VSSetShader(g_hudVs, nullptr, 0);
					context->PSSetShader(g_hudTexPs, nullptr, 0);
					context->PSSetSamplers(0, 1, &g_sampler);
					for (int k = 0; k < kHudPartCount; ++k) {
						if (k == 2 && g_menuShown.load() == 0) {
							continue;  // MORDHAU's menu is not open
						}
						if (k == 0 && !g_mordhauBars.load()) {
							continue;  // MORDHAU's health / stamina bars are not shown (sheets/builds.json mordhau_bars)
						}
						context->PSSetShader(k == 2 && g_hudPicturePs ? g_hudPicturePs : g_hudTexPs, nullptr, 0);
						context->PSSetShaderResources(0, 1, &g_hudSrv[k]);
						context->Draw(6, static_cast<UINT>(k * 6));
					}
					saved.Restore(context);
				}
				rtv->Release();
			}
			back->Release();
		}

		HRESULT STDMETHODCALLTYPE PresentHook(IDXGISwapChain* a_chain, UINT a_sync, UINT a_flags)
		{
			DrawHudOnBackBuffer(a_chain);
			return g_present(a_chain, a_sync, a_flags);
		}

		void EnsurePresentHook(RE::BSGraphics::Renderer* a_renderer, ID3D11Device* a_device, ID3D11DeviceContext* a_context)
		{
			if (g_presentHooked || !a_renderer) {
				return;
			}
			auto* chain = reinterpret_cast<IDXGISwapChain*>(a_renderer->GetRuntimeData().renderWindows[0].swapChain);
			if (!chain) {
				return;
			}
			g_presentDevice = a_device;
			g_presentContext = a_context;
			void** table = *reinterpret_cast<void***>(chain);
			DWORD  old = 0;
			if (VirtualProtect(&table[8], sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) {
				g_present = reinterpret_cast<PresentFn>(table[8]);
				table[8] = reinterpret_cast<void*>(&PresentHook);
				VirtualProtect(&table[8], sizeof(void*), old, &old);
				g_presentHooked = true;
				logger::info("character: MORDHAU's HUD is drawn on the finished frame");
			}
		}

		// MORDHAU's hit marker: four short slanted ticks around the crosshair for a quarter of a second.
		void DrawHitMarker(ID3D11DeviceContext* context, D3D11_TEXTURE2D_DESC& sd, D3D11_MAPPED_SUBRESOURCE& map, const UINT stride, const UINT offset)
		{
		if (const auto markedAt = g_hitMarkerAt.load(); markedAt != 0 && g_hudVs && g_hudPs && g_hudVb && g_depthOff) {
			const auto  nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			const float age = static_cast<float>(nowMs - markedAt) / 1000.0f;
			if (age > 0.28f) {
				g_hitMarkerAt.store(0);
			} else {
				const float w = static_cast<float>(sd.Width), h = static_cast<float>(sd.Height);
				const float grow = 1.0f + age * 1.6f;                       // (it opens a little as it fades)
				const float inner = 0.011f * h * grow, outer = 0.024f * h * grow, half = std::max(1.0f, 0.0016f * h);
				const float fade = 1.0f - age / 0.28f;
				const bool  kill = g_hitMarkerKill.load();
				const float r = fade, g = kill ? 0.12f * fade : fade, b = kill ? 0.10f * fade : fade;
				std::vector<Vertex> ticks;
				const auto point = [&](float x, float y) {
					Vertex v{};
					v.pos[0] = (w * 0.5f + x) / w * 2.0f - 1.0f;
					v.pos[1] = 1.0f - (h * 0.5f + y) / h * 2.0f;
					v.nrm[0] = r, v.nrm[1] = g, v.nrm[2] = b;
					ticks.push_back(v);
				};
				for (const auto& [sx, sy] : { std::pair{ 1.0f, 1.0f }, std::pair{ -1.0f, 1.0f }, std::pair{ 1.0f, -1.0f }, std::pair{ -1.0f, -1.0f } }) {
					const float d = 0.70710678f;
					const float ax = sx * inner * d, ay = sy * inner * d, bx = sx * outer * d, by = sy * outer * d;
					const float nx = -sy * d * half, ny = sx * d * half;   // across the tick
					point(ax + nx, ay + ny), point(bx + nx, by + ny), point(bx - nx, by - ny);
					point(ax + nx, ay + ny), point(bx - nx, by - ny), point(ax - nx, ay - ny);
				}
				if (SUCCEEDED(context->Map(g_hudVb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
					std::memcpy(map.pData, ticks.data(), ticks.size() * sizeof(Vertex));
					context->Unmap(g_hudVb, 0);
					const float factor[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
					context->OMSetBlendState(nullptr, factor, 0xFFFFFFFF);
					context->OMSetDepthStencilState(g_depthOff, 0);
					context->IASetInputLayout(g_layout);
					context->IASetVertexBuffers(0, 1, &g_hudVb, &stride, &offset);
					context->VSSetShader(g_hudVs, nullptr, 0);
					context->PSSetShader(g_hudPs, nullptr, 0);
					context->Draw(static_cast<UINT>(ticks.size()), 0);
				}
			}
		}
		}

		// Health and stamina bars drawn by the plugin itself, for when no picture of MORDHAU's window can be had - only
		// while MORDHAU's bars are shown at all (sheets/builds.json mordhau_bars). With them off, Skyrim's own bars
		// are the only ones: before, these showed while MORDHAU's window was still opening, and beside Skyrim's.
		void DrawFallbackBars(RE::PlayerCharacter* player, ID3D11DeviceContext* context, D3D11_TEXTURE2D_DESC& sd, const Link::State& link, D3D11_MAPPED_SUBRESOURCE& map, const UINT stride, const UINT offset, bool& hudStreamed)
		{
		if (g_mordhauBars.load() && !hudStreamed && link.live && g_hudVs && g_hudPs && g_hudVb && g_depthOff && player->AsActorState()->IsWeaponDrawn()) {
			auto*       values = player->AsActorValueOwner();
			const float healthMax = std::max(values->GetPermanentActorValue(RE::ActorValue::kHealth), 1.0f);
			const float health = std::clamp(values->GetActorValue(RE::ActorValue::kHealth) / healthMax, 0.0f, 1.0f);
			const float stamina = std::clamp(link.stamina / 100.0f, 0.0f, 1.0f);
			const float w = static_cast<float>(sd.Width), h = static_cast<float>(sd.Height);
			const float barW = 0.15f * w, barH = 0.012f * h, gap = 0.012f * w, y0 = 0.945f * h, border = std::max(1.0f, 0.0015f * h);
			std::vector<Vertex> quads;
			const auto quad = [&](float x0, float yTop, float x1, float yBottom, float r, float g, float b) {
				const float cx0 = x0 / w * 2.0f - 1.0f, cx1 = x1 / w * 2.0f - 1.0f, cy0 = 1.0f - yTop / h * 2.0f, cy1 = 1.0f - yBottom / h * 2.0f;
				const float corners[6][2]{ { cx0, cy0 }, { cx1, cy0 }, { cx1, cy1 }, { cx0, cy0 }, { cx1, cy1 }, { cx0, cy1 } };
				for (const auto& c : corners) {
					Vertex v{};
					v.pos[0] = c[0], v.pos[1] = c[1];
					v.nrm[0] = r, v.nrm[1] = g, v.nrm[2] = b;
					quads.push_back(v);
				}
			};
			const float mid = 0.5f * w;
			// frames, troughs, fills
			quad(mid - gap - barW - border, y0 - border, mid - gap + border, y0 + barH + border, 0.01f, 0.01f, 0.01f);
			quad(mid + gap - border, y0 - border, mid + gap + barW + border, y0 + barH + border, 0.01f, 0.01f, 0.01f);
			quad(mid - gap - barW, y0, mid - gap, y0 + barH, 0.07f, 0.06f, 0.03f);
			quad(mid + gap, y0, mid + gap + barW, y0 + barH, 0.07f, 0.03f, 0.03f);
			quad(mid - gap - barW * stamina, y0, mid - gap, y0 + barH, 0.85f, 0.66f, 0.10f);
			quad(mid + gap, y0, mid + gap + barW * health, y0 + barH, 0.62f, 0.05f, 0.05f);
			if (SUCCEEDED(context->Map(g_hudVb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
				std::memcpy(map.pData, quads.data(), quads.size() * sizeof(Vertex));
				context->Unmap(g_hudVb, 0);
				context->OMSetDepthStencilState(g_depthOff, 0);
				context->IASetVertexBuffers(0, 1, &g_hudVb, &stride, &offset);
				context->VSSetShader(g_hudVs, nullptr, 0);
				context->PSSetShader(g_hudPs, nullptr, 0);
				context->Draw(static_cast<UINT>(quads.size()), 0);
			}
		}
		}
	}
}
