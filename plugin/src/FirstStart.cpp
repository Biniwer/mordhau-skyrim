#include "FirstStart.h"

#include "bake/Cache.h"
#include "gen/Sheets.h"

#include <fstream>
#include <string>

namespace dm::FirstStart
{
	namespace
	{
		std::filesystem::path AnswerFile()
		{
			return mbake::DefaultCache().parent_path() / "understood.txt";
		}

		class Answered : public RE::IMessageBoxCallback
		{
		public:
			void Run(std::uint8_t a_button) override
			{
				if (a_button == 0) {
					std::error_code ec;
					std::filesystem::create_directories(AnswerFile().parent_path(), ec);
					std::ofstream out(AnswerFile(), std::ios::binary | std::ios::trunc);
					out << sheets::kFirstStartVersion;
					logger::info("first start: the player chose \"I understand\" (text version {}): MORDHAU may be started", sheets::kFirstStartVersion);
				} else {
					logger::info("first start: the player chose \"not now\": MORDHAU is not started; Skyrim plays as usual");
				}
			}
		};
	}

	bool Accepted()
	{
		std::ifstream in(AnswerFile(), std::ios::binary);
		int           version = 0;
		return (in >> version) && version >= sheets::kFirstStartVersion;
	}

	void ShowIfNeeded()
	{
		static bool shown = false;
		if (shown || Accepted()) {
			return;
		}
		auto* factories = RE::MessageDataFactoryManager::GetSingleton();
		auto* strings = RE::InterfaceStrings::GetSingleton();
		auto* creator = factories && strings ? factories->GetCreator<RE::MessageBoxData>(strings->messageBoxData) : nullptr;
		auto* box = creator ? creator->Create() : nullptr;
		if (!box) {
			logger::warn("first start: the message box could not be made - MORDHAU stays closed until it has been accepted");
			return;
		}
		shown = true;
		box->bodyText = sheets::kFirstStartBody;
		box->buttonText.push_back(sheets::kFirstStartAccept);
		box->buttonText.push_back(sheets::kFirstStartDecline);
		box->cancelButtonIndex = 1;
		box->isCancellable = true;
		box->verticalButtons = true;
		box->callback = RE::make_smart<Answered>();
		using func_t = void(RE::MessageBoxData*);
		REL::Relocation<func_t> queue{ RELOCATION_ID(51422, 52271) };  // MessageBoxData::QueueMessage
		queue(box);
		logger::info("first start: the message box about the risk is shown; MORDHAU waits for the answer");
	}
}
