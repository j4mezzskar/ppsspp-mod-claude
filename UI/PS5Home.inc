// The look of the PS5-style home screen: the layer between the game's artwork and the controls.
//
// This file is #included from MainScreen.cpp, so it needs no build-system changes. It draws:
//   - darkening over the artwork, stronger toward the bottom so tiles and text stay readable
//   - the clock
//   - the selected game's title and description (yours, if you've written one)
//   - the button hints along the bottom
// The artwork itself is drawn by MainScreen::DrawBackground, and the tiles by the game list.

#include <algorithm>
#include <ctime>
#include <string>

#include "Core/Config.h"
#include "UI/CustomArt.h"
#include "UI/GameInfoCache.h"

namespace ps5home {

static uint32_t Alpha(float a, uint32_t rgb = 0x00FFFFFF) {
	return ((uint32_t)(255.0f * std::clamp(a, 0.0f, 1.0f)) << 24) | (rgb & 0x00FFFFFF);
}

class HeroOverlay : public UI::View {
public:
	HeroOverlay(const Path *highlighted, UI::LayoutParams *layoutParams)
		: UI::View(layoutParams), highlighted_(highlighted) {}

	void GetContentDimensions(const UIContext &dc, float &w, float &h) const override {
		w = 0.0f;
		h = 0.0f;
	}

	void Draw(UIContext &dc) override {
		using namespace UI;
		const Bounds b = bounds_;
		const bool portrait = b.h > b.w;
		const double now = time_now_d();
		const float speed = std::max(0.1f, g_Config.iPS5AnimSpeed / 100.0f);

		dc.Draw()->Flush();
		dc.RebindTexture();

		// Dim the artwork a little, then darken it toward the bottom.
		const float dim = std::clamp(g_Config.iPS5HeroDim, 0, 90) / 100.0f;
		if (dim > 0.0f) {
			dc.FillRect(UI::Drawable(Alpha(dim, 0x00000000)), b);
		}
		const int steps = 28;
		const float gradH = b.h * (portrait ? 0.62f : 0.70f);
		for (int i = 0; i < steps; i++) {
			const float t = (float)i / (float)steps;
			dc.FillRect(UI::Drawable(Alpha(t * t * 0.85f, 0x00100805)),
				Bounds(b.x, b.y2() - gradH + gradH * t, b.w, gradH / steps + 1.0f));
		}

		if (g_Config.bPS5ShowClock) {
			char clock[16] = "";
			const time_t t = time(nullptr);
			if (const struct tm *lt = localtime(&t)) {
				strftime(clock, sizeof(clock), "%H:%M", lt);
			}
			dc.SetFontStyle(*GetTextStyle(dc, TextSize::Normal));
			dc.DrawText(clock, b.x + 30.0f, b.y + 22.0f, Alpha(0.95f), ALIGN_TOPLEFT);
		}

		const Path path = highlighted_ ? *highlighted_ : Path();
		if (path != shown_) {
			shown_ = path;
			shownSince_ = now;
		}

		// Where the carousel starts: tile row, the selected tile's title, headroom and the path bar.
		const float tileScale = g_Config.iPS5TileScale / 100.0f;
		const float carouselH = 80.0f * 1.35f * tileScale + 34.0f + 26.0f + 76.0f;
		const float footerH = g_Config.bPS5ShowHints ? 44.0f : 8.0f;
		const float margin = 30.0f;
		const float maxW = portrait ? b.w - margin * 2.0f : std::min(b.w * 0.55f, 640.0f);
		const float bottom = b.y2() - footerH - carouselH - 14.0f;

		if (!path.empty()) {
			std::shared_ptr<GameInfo> info = g_gameInfoCache->GetInfo(dc.GetDrawContext(), path, GameInfoFlags::PARAM_SFO);

			std::string title = info->GetTitle();
			for (char &c : title) {
				if (c == '\n') {
					c = ' ';
				}
			}

			const std::string key = CustomArt::KeyFor(path);
			if (key != descKey_ || descRev_ != CustomArt::Revision()) {
				descKey_ = key;
				descRev_ = CustomArt::Revision();
				desc_ = CustomArt::ReadDescription(key);
			}
			std::string desc = desc_;
			if (desc.empty() && info->Ready(GameInfoFlags::PARAM_SFO)) {
				// Nothing written yet, so show the game's ID. Hold a game to add your own.
				desc = info->id;
			}
			if (!g_Config.bPS5ShowDescription) {
				desc.clear();
			}

			// Fade and slide in whenever the selected game changes.
			const float p = std::clamp((float)((now - shownSince_) * 4.0 * speed), 0.0f, 1.0f);
			const float ease = 1.0f - (1.0f - p) * (1.0f - p);
			const float slide = (1.0f - ease) * 16.0f;

			const float descH = desc.empty() ? 0.0f : 78.0f;
			const float titleH = 96.0f;

			dc.SetFontStyle(*GetTextStyle(dc, TextSize::Big));
			const Bounds titleBounds(b.x + margin, bottom - descH - titleH + slide, maxW, titleH);
			dc.DrawTextRect(title, titleBounds.Offset(2.0f, 2.0f), Alpha(ease * 0.7f, 0x00000000), ALIGN_BOTTOMLEFT | FLAG_WRAP_TEXT);
			dc.DrawTextRect(title, titleBounds, Alpha(ease), ALIGN_BOTTOMLEFT | FLAG_WRAP_TEXT);

			if (!desc.empty()) {
				dc.SetFontStyle(*GetTextStyle(dc, TextSize::Small));
				const Bounds descBounds(b.x + margin, bottom - descH + 8.0f + slide, maxW, descH);
				dc.PushScissor(Bounds(descBounds.x, descBounds.y - slide, descBounds.w, descBounds.h));
				dc.DrawTextRect(desc, descBounds.Offset(1.0f, 1.0f), Alpha(ease * 0.7f, 0x00000000), ALIGN_TOPLEFT | FLAG_WRAP_TEXT);
				dc.DrawTextRect(desc, descBounds, Alpha(ease * 0.85f), ALIGN_TOPLEFT | FLAG_WRAP_TEXT);
				dc.PopScissor();
			}
		}

		if (g_Config.bPS5ShowHints) {
			dc.SetFontStyle(*GetTextStyle(dc, TextSize::Small));
			const char *hints = portrait
				? "Tap: select    Tap again: play    Hold: details"
				: "L1 / R1: categories        Tap: select        Tap again: play        Hold: details";
			dc.DrawText(hints, b.x2() - margin, b.y2() - 34.0f, Alpha(0.75f), ALIGN_TOPRIGHT);
		}

		dc.SetFontStyle(dc.GetTheme().uiFont);
	}

private:
	const Path *highlighted_;
	Path shown_;
	double shownSince_ = 0.0;
	std::string descKey_;
	int descRev_ = -1;
	std::string desc_;
};

}  // namespace ps5home
