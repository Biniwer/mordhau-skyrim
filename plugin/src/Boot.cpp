#include "Boot.h"
#include "FirstStart.h"

namespace dm
{
	namespace
	{
		BootEvents g_events;
		std::atomic_bool g_dataLoaded{ false };
		std::atomic_bool g_mainMenuClosed{ false };

		class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (!a_event) {
					return RE::BSEventNotifyControl::kContinue;
				}
				// Data loaded: the first menu event once the player's base form exists.
				if (!g_dataLoaded && RE::TESForm::LookupByID(0x7)) {
					g_dataLoaded = true;
					logger::info("boot: data loaded");
					if (g_events.dataLoaded) {
						g_events.dataLoaded();
					}
				}
				if (a_event->menuName == RE::MainMenu::MENU_NAME && !a_event->opening) {
					g_mainMenuClosed = true;
				} else if (a_event->menuName == RE::MainMenu::MENU_NAME && a_event->opening) {
					g_mainMenuClosed = false;
					FirstStart::ShowIfNeeded();  // (sheets/first_start.json: MORDHAU is not started before "I understand")
				}
				// New game, or a save chosen from the main menu: the loading menu closing after the main menu closed.
				if (a_event->menuName == RE::LoadingMenu::MENU_NAME && !a_event->opening && g_mainMenuClosed.exchange(false)) {
					logger::info("boot: game loaded (from main menu)");
					if (g_events.gameLoaded) {
						g_events.gameLoaded();
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class LoadSink : public RE::BSTEventSink<RE::TESLoadGameEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent*, RE::BSTEventSource<RE::TESLoadGameEvent>*) override
			{
				// A save loaded while already in game (the main-menu path is handled by MenuSink).
				if (!g_mainMenuClosed) {
					logger::info("boot: game loaded (in game)");
					if (g_events.gameLoaded) {
						g_events.gameLoaded();
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		MenuSink g_menuSink;
		LoadSink g_loadSink;
	}

	void InstallBoot(const BootEvents& a_events)
	{
		g_events = a_events;
		RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&g_menuSink);
		if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
			holder->AddEventSink<RE::TESLoadGameEvent>(&g_loadSink);
		}
	}
}
