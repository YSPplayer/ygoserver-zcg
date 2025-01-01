#include "data_manager.h"
#include "game.h"
#include <stdio.h>
#if !defined(YGOPRO_SERVER_MODE) || defined(SERVER_ZIP_SUPPORT)
#include "spmemvfs/spmemvfs.h"
#endif

namespace ygo {

	const wchar_t* DataManager::unknown_string = L"???";
	byte DataManager::scriptBuffer[0x20000];
#if !defined(YGOPRO_SERVER_MODE) || defined(SERVER_ZIP_SUPPORT)
	IFileSystem* DataManager::FileSystem;
#endif
	DataManager dataManager;
	///zdiy///
	ZCGManager zcgManager;
	///zdiy///
	DataManager::DataManager() : _datas(32768), _strings(32768) {
		extra_setcode = { {8512558u, {0x8f, 0x54, 0x59, 0x82, 0x13a}}, };
	}
	bool DataManager::ReadDB(sqlite3* pDB) {
		sqlite3_stmt* pStmt{};
		///zdiy///
		//#ifdef YGOPRO_SERVER_MODE
		//	const char* sql = "select * from datas";
		//#else
		//	const char* sql = "select * from datas,texts where datas.id=texts.id";
		//#endif
		const char* sql = "select * from datas,texts where datas.id=texts.id";
		///zdiy///
		if (sqlite3_prepare_v2(pDB, sql, -1, &pStmt, 0) != SQLITE_OK)
			return Error(pDB);

		///zdiy///
		//#ifndef YGOPRO_SERVER_MODE
		//	wchar_t strBuffer[4096];
		//#endif
		wchar_t strBuffer[4096];
		///zdiy///
		int step = 0;
		do {
			CardDataC cd;
			CardString cs;
			step = sqlite3_step(pStmt);
			if (step == SQLITE_BUSY || step == SQLITE_ERROR || step == SQLITE_MISUSE)
				return Error(pDB, pStmt);
			else if (step == SQLITE_ROW) {
				cd.code = sqlite3_column_int(pStmt, 0);
				cd.ot = sqlite3_column_int(pStmt, 1);
				cd.alias = sqlite3_column_int(pStmt, 2);
				auto setcode = sqlite3_column_int64(pStmt, 3);
				if (setcode) {
					auto it = extra_setcode.find(cd.code);
					if (it != extra_setcode.end()) {
						int len = it->second.size();
						if (len > SIZE_SETCODE)
							len = SIZE_SETCODE;
						if (len)
							std::memcpy(cd.setcode, it->second.data(), len * sizeof(uint16_t));
					}
					else
						cd.set_setcode(setcode);
				}
				cd.type = sqlite3_column_int(pStmt, 4);
				cd.attack = sqlite3_column_int(pStmt, 5);
				cd.defense = sqlite3_column_int(pStmt, 6);
				if (cd.type & TYPE_LINK) {
					cd.link_marker = cd.defense;
					cd.defense = 0;
				}
				else
					cd.link_marker = 0;
				unsigned int level = sqlite3_column_int(pStmt, 7);
				cd.level = level & 0xff;
				cd.lscale = (level >> 24) & 0xff;
				cd.rscale = (level >> 16) & 0xff;
				cd.race = sqlite3_column_int(pStmt, 8);
				cd.attribute = sqlite3_column_int(pStmt, 9);
				cd.category = sqlite3_column_int(pStmt, 10);
				int rootAttack = cd.attack;
				int rootDefense = cd.defense;
				if (cd.attack >= 999999) cd.attack = 10000;//无限大最大值限制
				if (cd.defense >= 999999) cd.defense = 10000;//无限大最大值限制
				_datas[cd.code] = cd;
				///zdiy///
				if (const char* text = (const char*)sqlite3_column_text(pStmt, 12)) {
					BufferIO::DecodeUTF8(text, strBuffer);
					if (cd.code >= ZCG_MIN_CODE && cd.code <= ZCG_MAX_CODE && ZUtil::EndsWithZCG(std::wstring(strBuffer))) { //是Z卡
						//不添加衍生物
						if (cd.type & TYPE_TOKEN) continue;
						int code = cd.code;
						//不添加禁止卡
						if (std::find(zcgManager.ban_codes.begin(), zcgManager.ban_codes.end(), cd.code) != zcgManager.ban_codes.end())continue;
						/*	cd.type& TYPES_EXTRA_DECK ? zcgManager.extra_datas[]*/
							//不添加限制类型泛用卡
						if (std::find(zcgManager.universal_codes_limit.begin(), zcgManager.universal_codes_limit.end(), cd.code) != zcgManager.universal_codes_limit.end())continue;
						//添加全数额外卡片
						if (cd.type & TYPES_EXTRA_DECK) {//添加到额外卡组的卡片
							zcgManager._codes.extraMonsterCodes.push_back(code);
						}
						else if (cd.type & TYPE_FIELD) {//添加场地魔法
							zcgManager.field_codes.push_back(code);
						}
						//添加系列卡+泛用卡+非泛用卡
						zcgManager._zdatas.push_back(&_datas[cd.code]);
						bool get_setcode = false;
						for (const auto& pair : zcgManager.setcode_map) {//添加指定系列的卡
							auto setcode = pair.first;
							auto index = pair.second;
							if (index == ZCGSetCodeType::Custom) continue;//自定义过滤
							if (cd.is_setcode(setcode)) {
								auto& group = zcgManager.stype_codes[index];
								if (cd.type & TYPES_EXTRA_DECK) {//添加到额外卡组的卡片
									group.extraMonsterCodes.push_back(code);
								}
								else if (cd.type & (TYPE_SPELL | TYPE_TRAP)) {//添加魔陷
									group.mainSpellCodes.push_back(code);
								}
								else if (cd.type & TYPE_MONSTER) {
									cd.level > 4 ? group.main4UpMonsterCodes.push_back(code) ://添加等级4以上
										group.main4BelowMonsterCodes.push_back(code);//添加等级4以下
								}
								else continue;
								get_setcode = true;
								break;
							}
						}
						//不添加泛用卡
						if (std::find(zcgManager.universal_codes.begin(), zcgManager.universal_codes.end(), cd.code) != zcgManager.universal_codes.end())continue;
						if (!get_setcode) {//都没有则添加到常规卡片
							if (cd.type & (TYPE_SPELL | TYPE_TRAP)) {//添加魔陷
								zcgManager._codes.mainSpellCodes.push_back(code);
							}
							else if (cd.type & TYPE_MONSTER) {
								cd.level > 4 ? zcgManager._codes.main4UpMonsterCodes.push_back(code) ://添加等级4以上
									zcgManager._codes.main4BelowMonsterCodes.push_back(code);//添加等级4以下
							}
						}
					}
				}
				///zdiy///
#ifndef YGOPRO_SERVER_MODE
				if (const char* text = (const char*)sqlite3_column_text(pStmt, 12)) {
					BufferIO::DecodeUTF8(text, strBuffer);
					cs.name = strBuffer;
				}
				if (const char* text = (const char*)sqlite3_column_text(pStmt, 13)) {
					BufferIO::DecodeUTF8(text, strBuffer);
					cs.text = strBuffer;
				}
				for (int i = 0; i < 16; ++i) {
					if (const char* text = (const char*)sqlite3_column_text(pStmt, i + 14)) {
						BufferIO::DecodeUTF8(text, strBuffer);
						cs.desc[i] = strBuffer;
					}
				}
				_strings[cd.code] = cs;
#endif //YGOPRO_SERVER_MODE
			}
		} while (step != SQLITE_DONE);
		sqlite3_finalize(pStmt);
		return true;
	}
	bool DataManager::LoadDB(const wchar_t* wfile) {
		char file[256];
		BufferIO::EncodeUTF8(wfile, file);
#if defined(YGOPRO_SERVER_MODE) && !defined(SERVER_ZIP_SUPPORT)
		bool ret{};
		sqlite3* pDB{};
		if (sqlite3_open_v2(file, &pDB, SQLITE_OPEN_READONLY, 0) != SQLITE_OK)
			ret = Error(pDB);
		else
			ret = ReadDB(pDB);
		sqlite3_close(pDB);
#else
#ifdef _WIN32
		IReadFile* reader = FileSystem->createAndOpenFile(wfile);
#else
		IReadFile* reader = FileSystem->createAndOpenFile(file);
#endif
		if (reader == NULL)
			return false;
		spmemvfs_db_t db;
		spmembuffer_t* mem = (spmembuffer_t*)calloc(sizeof(spmembuffer_t), 1);
		spmemvfs_env_init();
		mem->total = mem->used = reader->getSize();
		mem->data = (char*)malloc(mem->total + 1);
		reader->read(mem->data, mem->total);
		reader->drop();
		(mem->data)[mem->total] = '\0';
		bool ret{};
		if (spmemvfs_open_db(&db, file, mem) != SQLITE_OK)
			ret = Error(db.handle);
		else
			ret = ReadDB(db.handle);
		spmemvfs_close_db(&db);
		spmemvfs_env_fini();
#endif //YGOPRO_SERVER_MODE
		return ret;
	}
	bool DataManager::LoadStrings(const char* file) {
		FILE* fp = fopen(file, "r");
		if (!fp)
			return false;
		char linebuf[256];
		while (fgets(linebuf, 256, fp)) {
			ReadStringConfLine(linebuf);
		}
		fclose(fp);
		for (int i = 0; i < 301; ++i)
			myswprintf(numStrings[i], L"%d", i);
		return true;
	}
#ifndef YGOPRO_SERVER_MODE
	bool DataManager::LoadStrings(IReadFile* reader) {
		char ch[2] = " ";
		char linebuf[256] = "";
		while (reader->read(&ch[0], 1)) {
			if (ch[0] == '\0')
				break;
			std::strcat(linebuf, ch);
			if (ch[0] == '\n') {
				ReadStringConfLine(linebuf);
				linebuf[0] = '\0';
			}
		}
		reader->drop();
		return true;
	}
#endif //YGOPRO_SERVER_MODE
	void DataManager::ReadStringConfLine(const char* linebuf) {
		if (linebuf[0] != '!')
			return;
		char strbuf[256]{};
		int value{};
		wchar_t strBuffer[4096]{};
		if (sscanf(linebuf, "!%63s", strbuf) != 1)
			return;
		if (!std::strcmp(strbuf, "system")) {
			if (sscanf(&linebuf[7], "%d %240[^\n]", &value, strbuf) != 2)
				return;
			BufferIO::DecodeUTF8(strbuf, strBuffer);
			_sysStrings[value] = strBuffer;
		}
		else if (!std::strcmp(strbuf, "victory")) {
			if (sscanf(&linebuf[8], "%x %240[^\n]", &value, strbuf) != 2)
				return;
			BufferIO::DecodeUTF8(strbuf, strBuffer);
			_victoryStrings[value] = strBuffer;
		}
		else if (!std::strcmp(strbuf, "counter")) {
			if (sscanf(&linebuf[8], "%x %240[^\n]", &value, strbuf) != 2)
				return;
			BufferIO::DecodeUTF8(strbuf, strBuffer);
			_counterStrings[value] = strBuffer;
		}
		else if (!std::strcmp(strbuf, "setname")) {
			//using tab for comment
			if (sscanf(&linebuf[8], "%x %240[^\t\n]", &value, strbuf) != 2)
				return;
			BufferIO::DecodeUTF8(strbuf, strBuffer);
			_setnameStrings[value] = strBuffer;
		}
	}
	bool DataManager::Error(sqlite3* pDB, sqlite3_stmt* pStmt) {
		errmsg[0] = '\0';
		std::strncat(errmsg, sqlite3_errmsg(pDB), sizeof errmsg - 1);
		if (pStmt)
			sqlite3_finalize(pStmt);
		return false;
	}
	code_pointer DataManager::GetCodePointer(unsigned int code) const {
		return _datas.find(code);
	}
	string_pointer DataManager::GetStringPointer(unsigned int code) const {
		return _strings.find(code);
	}
	code_pointer DataManager::datas_begin() {
		return _datas.cbegin();
	}
	code_pointer DataManager::datas_end() {
		return _datas.cend();
	}
	string_pointer DataManager::strings_begin() {
		return _strings.cbegin();
	}
	string_pointer DataManager::strings_end() {
		return _strings.cend();
	}
	bool DataManager::GetData(unsigned int code, CardData* pData) const {
		auto cdit = _datas.find(code);
		if (cdit == _datas.end())
			return false;
		if (pData) {
			*pData = cdit->second;
		}
		return true;
	}
	bool DataManager::GetString(unsigned int code, CardString* pStr) const {
		auto csit = _strings.find(code);
		if (csit == _strings.end()) {
			pStr->name = unknown_string;
			pStr->text = unknown_string;
			return false;
		}
		*pStr = csit->second;
		return true;
	}
	const wchar_t* DataManager::GetName(unsigned int code) const {
		auto csit = _strings.find(code);
		if (csit == _strings.end())
			return unknown_string;
		if (!csit->second.name.empty())
			return csit->second.name.c_str();
		return unknown_string;
	}
	const wchar_t* DataManager::GetText(unsigned int code) const {
		auto csit = _strings.find(code);
		if (csit == _strings.end())
			return unknown_string;
		if (!csit->second.text.empty())
			return csit->second.text.c_str();
		return unknown_string;
	}
	const wchar_t* DataManager::GetDesc(unsigned int strCode) const {
		if (strCode < (MIN_CARD_ID << 4))
			return GetSysString(strCode);
		unsigned int code = (strCode >> 4) & 0x0fffffff;
		unsigned int offset = strCode & 0xf;
		auto csit = _strings.find(code);
		if (csit == _strings.end())
			return unknown_string;
		if (!csit->second.desc[offset].empty())
			return csit->second.desc[offset].c_str();
		return unknown_string;
	}
	const wchar_t* DataManager::GetSysString(int code) const {
		if (code < 0 || code > MAX_STRING_ID)
			return unknown_string;
		auto csit = _sysStrings.find(code);
		if (csit == _sysStrings.end())
			return unknown_string;
		return csit->second.c_str();
	}
	const wchar_t* DataManager::GetVictoryString(int code) const {
		auto csit = _victoryStrings.find(code);
		if (csit == _victoryStrings.end())
			return unknown_string;
		return csit->second.c_str();
	}
	const wchar_t* DataManager::GetCounterName(int code) const {
		auto csit = _counterStrings.find(code);
		if (csit == _counterStrings.end())
			return unknown_string;
		return csit->second.c_str();
	}
	const wchar_t* DataManager::GetSetName(int code) const {
		auto csit = _setnameStrings.find(code);
		if (csit == _setnameStrings.end())
			return nullptr;
		return csit->second.c_str();
	}
	std::vector<unsigned int> DataManager::GetSetCodes(std::wstring setname) const {
		std::vector<unsigned int> matchingCodes;
		for (auto csit = _setnameStrings.begin(); csit != _setnameStrings.end(); ++csit) {
			auto xpos = csit->second.find_first_of(L'|');//setname|another setname or extra info
			if (setname.size() < 2) {
				if (csit->second.compare(0, xpos, setname) == 0
					|| csit->second.compare(xpos + 1, csit->second.length(), setname) == 0)
					matchingCodes.push_back(csit->first);
			}
			else {
				if (csit->second.substr(0, xpos).find(setname) != std::wstring::npos
					|| csit->second.substr(xpos + 1).find(setname) != std::wstring::npos) {
					matchingCodes.push_back(csit->first);
				}
			}
		}
		return matchingCodes;
	}
	const wchar_t* DataManager::GetNumString(int num, bool bracket) {
		if (!bracket)
			return numStrings[num];
		wchar_t* p = numBuffer;
		*p++ = L'(';
		BufferIO::CopyWStrRef(numStrings[num], p, 4);
		*p = L')';
		*++p = 0;
		return numBuffer;
	}
	const wchar_t* DataManager::FormatLocation(int location, int sequence) const {
		if (location == LOCATION_SZONE) {
			if (sequence < 5)
				return GetSysString(1003);
			else if (sequence == 5)
				return GetSysString(1008);
			else
				return GetSysString(1009);
		}
		int i = 1000;
		int string_id = 0;
		for (unsigned filter = LOCATION_DECK; filter <= LOCATION_PZONE; filter <<= 1, ++i) {
			if (filter == location) {
				string_id = i;
				break;
			}
		}
		if (string_id)
			return GetSysString(string_id);
		else
			return unknown_string;
	}
	const wchar_t* DataManager::FormatAttribute(int attribute) {
		wchar_t* p = attBuffer;
		unsigned filter = 1;
		int i = 1010;
		for (; filter != 0x80; filter <<= 1, ++i) {
			if (attribute & filter) {
				BufferIO::CopyWStrRef(GetSysString(i), p, 16);
				*p = L'|';
				*++p = 0;
			}
		}
		if (p != attBuffer)
			*(p - 1) = 0;
		else
			return unknown_string;
		return attBuffer;
	}
	const wchar_t* DataManager::FormatRace(int race) {
		wchar_t* p = racBuffer;
		unsigned filter = 1;
		int i = 1020;
		for (; filter < (1 << RACES_COUNT); filter <<= 1, ++i) {
			if (race & filter) {
				BufferIO::CopyWStrRef(GetSysString(i), p, 16);
				*p = L'|';
				*++p = 0;
			}
		}
		if (p != racBuffer)
			*(p - 1) = 0;
		else
			return unknown_string;
		return racBuffer;
	}
	const wchar_t* DataManager::FormatType(int type) {
		wchar_t* p = tpBuffer;
		unsigned filter = 1;
		int i = 1050;
		for (; filter != 0x8000000; filter <<= 1, ++i) {
			if (type & filter) {
				BufferIO::CopyWStrRef(GetSysString(i), p, 16);
				*p = L'|';
				*++p = 0;
			}
		}
		if (p != tpBuffer)
			*(p - 1) = 0;
		else
			return unknown_string;
		return tpBuffer;
	}
	const wchar_t* DataManager::FormatSetName(const uint16_t setcode[]) {
		wchar_t* p = scBuffer;
		for (int i = 0; i < 10; ++i) {
			if (!setcode[i])
				break;
			const wchar_t* setname = GetSetName(setcode[i]);
			if (setname) {
				BufferIO::CopyWStrRef(setname, p, 32);
				*p = L'|';
				*++p = 0;
			}
		}
		if (p != scBuffer)
			*(p - 1) = 0;
		else
			return unknown_string;
		return scBuffer;
	}
	const wchar_t* DataManager::FormatLinkMarker(int link_marker) {
		wchar_t* p = lmBuffer;
		*p = 0;
		if (link_marker & LINK_MARKER_TOP_LEFT)
			BufferIO::CopyWStrRef(L"[\u2196]", p, 4);
		if (link_marker & LINK_MARKER_TOP)
			BufferIO::CopyWStrRef(L"[\u2191]", p, 4);
		if (link_marker & LINK_MARKER_TOP_RIGHT)
			BufferIO::CopyWStrRef(L"[\u2197]", p, 4);
		if (link_marker & LINK_MARKER_LEFT)
			BufferIO::CopyWStrRef(L"[\u2190]", p, 4);
		if (link_marker & LINK_MARKER_RIGHT)
			BufferIO::CopyWStrRef(L"[\u2192]", p, 4);
		if (link_marker & LINK_MARKER_BOTTOM_LEFT)
			BufferIO::CopyWStrRef(L"[\u2199]", p, 4);
		if (link_marker & LINK_MARKER_BOTTOM)
			BufferIO::CopyWStrRef(L"[\u2193]", p, 4);
		if (link_marker & LINK_MARKER_BOTTOM_RIGHT)
			BufferIO::CopyWStrRef(L"[\u2198]", p, 4);
		return lmBuffer;
	}
	uint32 DataManager::CardReader(uint32 code, card_data* pData) {
		if (!dataManager.GetData(code, pData))
			pData->clear();
		return 0;
	}
	byte* DataManager::ScriptReaderEx(const char* script_name, int* slen) {
		// default script name: ./script/c%d.lua
#ifdef YGOPRO_SERVER_MODE
		char first[256]{};
		char second[256]{};
		char third[256]{};
		snprintf(first, sizeof first, "specials/%s", script_name + 9);
		snprintf(second, sizeof second, "expansions/%s", script_name + 2);
		snprintf(third, sizeof third, "%s", script_name + 2);
		if (ScriptReader(first, slen))
			return scriptBuffer;
		else if (ScriptReader(second, slen))
			return scriptBuffer;
		else
			return ScriptReader(third, slen);
#else
		char first[256]{};
		char second[256]{};
		if (mainGame->gameConf.prefer_expansion_script) {
			snprintf(first, sizeof first, "expansions/%s", script_name + 2);
			snprintf(second, sizeof second, "%s", script_name + 2);
		}
		else {
			snprintf(first, sizeof first, "%s", script_name + 2);
			snprintf(second, sizeof second, "expansions/%s", script_name + 2);
		}
		if (ScriptReader(first, slen))
			return scriptBuffer;
		else
			return ScriptReader(second, slen);
#endif //YGOPRO_SERVER_MODE
	}
	byte* DataManager::ScriptReader(const char* script_name, int* slen) {
#if defined(YGOPRO_SERVER_MODE) && !defined(SERVER_ZIP_SUPPORT)
		FILE* fp = fopen(script_name, "rb");
		if (!fp)
			return 0;
		int len = fread(scriptBuffer, 1, sizeof(scriptBuffer), fp);
		fclose(fp);
		if (len >= sizeof(scriptBuffer))
			return 0;
		*slen = len;
#else
#ifdef _WIN32
		wchar_t fname[256]{};
		BufferIO::DecodeUTF8(script_name, fname);
		IReadFile* reader = FileSystem->createAndOpenFile(fname);
#else
		IReadFile* reader = FileSystem->createAndOpenFile(script_name);
#endif
		if (reader == NULL)
			return 0;
		size_t size = reader->getSize();
		if (size > sizeof(scriptBuffer)) {
			reader->drop();
			return 0;
		}
		reader->read(scriptBuffer, size);
		reader->drop();
		*slen = size;
#endif //YGOPRO_SERVER_MODE
		return scriptBuffer;
	}
	///zdiy///
	std::random_device ZCGManager::rd;
	std::mt19937 ZCGManager::gen(rd());
	ZCGManager::ZCGManager() {
		card_type_contrasts = { {7,3},{6,4},{5,5},{4,6},{3,7} };
		stype_codes.resize(ZCGSetCodeType::MAX);
		setcode_map = {
			{ 0xa13,ZCGSetCodeType::Hades},
			{ 0xa33,ZCGSetCodeType::BTBlackFeather},
			{ 0xa35,ZCGSetCodeType::Up},
			{ 0xa50,ZCGSetCodeType::Olekarks},
			{ 0xa51,ZCGSetCodeType::Olekarks},
			{ 0xa60 ,ZCGSetCodeType::Martyr},
			{ 0xa70,ZCGSetCodeType::SStar},
			{ 0xa71,ZCGSetCodeType::SStar},
			{ 0xa80,ZCGSetCodeType::SuperGirl},
			{ 0xa81,ZCGSetCodeType::SuperGirl},
			{ 0xa90,ZCGSetCodeType::AngerPlants},
			{ 0xa100,ZCGSetCodeType::Osiris},
			{ 0xa110,ZCGSetCodeType::Armor},
			{ 0xa120,ZCGSetCodeType::Ancient},
			{ 0xa121,ZCGSetCodeType::Ancient},
			{ 0xa130,ZCGSetCodeType::PrimitiveInsects},
			{ 0xa140,ZCGSetCodeType::MechanicalInsects},
			{ 0xa160,ZCGSetCodeType::SoulBreaker},
			{ 0xa170,ZCGSetCodeType::RareAnimals},
			{ 0xa200,ZCGSetCodeType::GodOfficial},
			{ 0xa210,ZCGSetCodeType::SunGod},
			{ 0xa220,ZCGSetCodeType::Obelisk},
			{ 0xa250,ZCGSetCodeType::Immortal},
			{ 0xdb97,ZCGSetCodeType::Tianlong},
			{ 0xdb98,ZCGSetCodeType::Tianlong},
			{ 0xdb96,ZCGSetCodeType::Tianlong},
			{ 0xcf1,ZCGSetCodeType::CF},
			{ 0xae0,ZCGSetCodeType::HArmor},
			{ 0x21,ZCGSetCodeType::GodEarth},
			{ 0xa150,ZCGSetCodeType::Yaoge},
			{ 0x62,ZCGSetCodeType::Cartoon},
			{ 0x0,ZCGSetCodeType::Custom}
		};
		contrast_map = {
			{ZCGSetCodeType::Hades,{3,7}},
			{ZCGSetCodeType::BTBlackFeather,{4,6}},
			{ZCGSetCodeType::Up,{5,5}},
			{ZCGSetCodeType::Olekarks,{6,4}},//8,2
			{ZCGSetCodeType::Martyr,{5,5}},
			{ZCGSetCodeType::SStar,{4,6}},
			{ZCGSetCodeType::SuperGirl,{3,7}},
			{ZCGSetCodeType::AngerPlants,{4,6}},
			{ZCGSetCodeType::Armor,{5,5}},
			{ZCGSetCodeType::Ancient,{4,6}},
			{ZCGSetCodeType::PrimitiveInsects,{3,7}},
			{ZCGSetCodeType::MechanicalInsects,{3,7}},
			{ZCGSetCodeType::SoulBreaker,{5,5}},
			{ZCGSetCodeType::RareAnimals,{3,7}},
			{ZCGSetCodeType::GodOfficial,{2,8}},
			{ZCGSetCodeType::SunGod,{5,5}},
			{ZCGSetCodeType::Obelisk,{5,5}},
			{ZCGSetCodeType::Immortal,{3,7}},
			{ZCGSetCodeType::Tianlong,{3,7}},
			{ZCGSetCodeType::CF,{2,8}},
			{ZCGSetCodeType::HArmor,{2,8}},
			{ZCGSetCodeType::GodEarth,{3,7}},
			{ZCGSetCodeType::Yaoge,{3,7}},
			{ZCGSetCodeType::Cartoon,{4,6}},
			{ZCGSetCodeType::Custom,{8,2}},//这里的非常规卡占比将会随机所有系列的卡池
		};
		//卡组中只能存在一张的卡
		filter_limit_one_codes = {};
		//卡组中只存在两张的卡
		filter_limit_two_codes = { 77240149,77240142,77239934,77239636, 77239630,77239621,77239615,77239541,77239430,77239358,77239247, 77238003,77238004,77238010,77238012,77238015,77238016,77238021,77238032,77238033,77238034
		,77238041,77238077,77238097,77238099,77238184,77238187,77238194,77238197,77238244,77238254,77238279,77238278,
			77238300,77238328,77238341,77239059,77239064,77239563,77239564,
			77239565,77239677,77239678,77240346,77240384,
		};
		//卡组中可以存在三张的卡
		filter_limit_three_codes = {};
		//不允许存在的卡
		filter_limit_zero_codes = { 77238206,77239239,77240299,77240605,77239210, 77239113,77239960,77239979,77239978,77239998,77239996,77239995,77239123,77239122,77240599,77239983,77239985,77238210,77240160,77238234,77239020,77240691,77240688,77240610,77240601,77240581,77240548,77240523,77240520,77240502,77240501,77240493,77240486,77240485,77240483,77240472,77240471,77240470,77240464,77240440,
			77240441,77240423,77240422, 77239586,77239314, 77239175 ,77239693 ,77239799,77239695,77238297,77238163,77239994,77240005,77240283, 77238264,77238230,77240696, 77240581, 77240473, 77240422,77240348,77240341, 77240334, 77240303, 77240295, 77240294, 77240272,77240255,77240254,77240253, 77240252,77240228, 77240221, 77240219,77240214, 77240201, 77240200,77240198,77240151,77240136,77240072,77240044,77240007, 77240003, 77239998,77239997, 77239949, 77239943,77239944,77239945,77239929,77239925,77239922,77239917,77239918,77239919,77239905,77239901,77239679,77239676, 77239675,77239585,77239566,77239560,77239395, 77239359,77239349,77239345,77239291,77239290,77239280,77239279,77239268,77239267,77239264,
			77239265,77239255,77239233,77239232,77239150,77239147,77239144,
			77239090,77239078,77239063,77239008,77239007,77239002,77238325,77238229,77238227,77238179,77238176,77238170,77238171,77238172 };
		//系列可泛用卡
		universal_codes_setcode = { 77239622,77239103,77240694,77240692,77240690,77240689,77240687,77240666,77240652,77240651,77240648,77240645,77240644,77240639,77240631, 77240630,77240626,77240622,77240620,77240600,77240591,77240570,77240554,77240545,77240544,77240538,77240537,77240536,77240535,77240534,77240531,77240529,77240526,77240524,77240522,77240521,77240519,77240518,77240517,77240512,77240511,77240508,77240506,77240505,77240504,77240500,77240494,77240492,77240491, 77240489,77240474,77240469, 77240466,77240465,77240463,77240460,77240458,77240451,77240449,77240447,77240446,77240445,77240442,77240419,77240416, 77240484, 77240386,77240385, 77240374, 77240368,77240358,77240354,77240353,77240350,77240349,77240336, 77240323, 77240321, 77240320,77240316,77240314,77240306, 77240305, 77240304, 77240290,77240288,77240282,77240281, 77240280,77240274, 77240271,77240270,77240265, 77240264,77240261,77240259, 77240258,77240257,77240251,77240250,77240248, 77240247, 77240246, 77240245, 77240244, 77240242,77240241,77240235,77240231,77240210,77240209,77240189,77240183,77240178,77240177,77240175,77240171,77240168,77240165, 77240164, 77240163,77240162,77240156,77240155, 77240154,77240153,77240152,77240132,77240127, 77240126, 77240125,77240123,77240121,77240119, 77240118, 77240117,77240037,77240035,77240034,77240031, 777240029,7240028,77239916,77239891, 77239890,77239700,77239701,77239710,77239711,
			77239717,77239718,77239719,77239720,77239721,77239722,77239723,
			77239724,77239725,77239637, 77239628,77239627,77239624,77239615,77239562,77239561,77239550,77239541,77239534,77239535,77239536,77239537,77239525,77239526,77239526,77239528,77239529,77239530,77239515,77239516,77239517,77239520,77239511,77239512,77239513,77239509,77239510,77239506,77239507,
			77239508,77239503,77239418,77239417,77239404,77239403,77239402,77239401,77239390,77239388,77239384,77239357,77239356,77239352,77239346,77239325,77239324, 77239323,77239283, 77239261, 77239247,77239246,77239241,77239236,77239228,77239227,77239214,77239211,77239212,77239210,77238277,77238176,77238100 ,77238102,77238103,77238104,77238105,77238106,
		77238107,77238109,77238117,77238145,77238146,77238184,77238190,77238194,77238197,77238201,
			77238216,77238240,77238243,77238245,77238246,77238247,77238248,77238249,77238250,77238253,
			77238254,77238266,77238286,77238298,77238313,77238314,77238315,77238318,77238321,77238324,
			77238326,77238329,77238330,77238333,77238334,77238335,77238336,77238340,77238361,77238363,
			77238364,77238379,77238501,77238502,77238503,77238504,77238505,77238506,77238705,77238787,
			77239047,77239048,77239049,77239051,77239053,77239054,77239061,77239179,77239200,77239202,
			77239223,77239256,77239328,77239331,77239332,77240152,


		};
		ban_codes = { 77240315,77239149,77238206,77239239,77240299,77240605,77239210,77239113,77239960,77239979,77239978,77239997,77239998,77239996,77239995,77239123,77239122,77240599,77239983,77239985,77238210,77240160,77238234,77239020,77240691,77240688,77240610,77240548,77240523,77240520,77240502, 77240501,77240493,77240486, 77240485,77240471,77240470,77240440,
			77240441,77240423,77240422,77239586,77239314, 77239175 ,77239693 ,77239799,77239695,77238297,77238163,77239994,77240005,77240283,77238264,77238230,77240581,77240483,77240475, 77240473,77240472,77240439,77240433, 77240422, 77240391,77240348,77240341,77240334,77240295, 77240294, 77240278,77240279,77240272,77240253, 77240252, 77240228,77240214,77240201,77240200,77240198,77240195,77240196, 77240151,77240135,77240053,77240045,77240044,77240007,77240003,77239999,77239998, 77239997,77239940,77239941,77239942,77239934,77239925,77239920,77239902,77239905,77239676,77239675,77239585,77239566, 77239560,77239395,77239359,77239349,77239345,77239292,77239291,77239290,77239280,77239279, 77239268,77239267,77239264,77239265,77239255,77239235,77239233,77239232,77239230,77239204,77239194,77239155,77239154,77239153,77239150,77239147, 77239144,77239142, 77239133,77239116,77239090,77239088,77239078,77239063,77239008,77239007,77239005,77239006,77239002,77238325,77238229,77238227,77238179,77238172,77238170,77238171,77238063,77240679,77240680,77240681,77238028,77239142,77239144,77239145,77239180,77239181,77239182,
		77239183,77239184,77239268,77239660,77239661,
		77239662 ,77239663 ,77239664 ,77239665,77239666,77239675,77239676
		,77239679 ,77239684,77239688 ,77239895 ,77239896,77239897,77239900
		,77239901,77239902 ,77239903,77239904,77239909,77239917 ,77239918,
		77239919,77239920,77239922,77239929,77239931,77239932,77239933,
		 77239943,77239944,77239945,77239947,77239949,77239979,
		77239992 ,77239993,77240003,77240024,77240045,
		77240072,77240076,77240102,77240136,77240195,77240196,77240200,
		77240201,77240202,77240203,77240204,77240219,77240253 ,77240255 ,
		77240303,77240334,77240348,77240464,77240472, 77240473
		,77240475,77240479,77240478,77240480,77240481,77240483,77240601,77240658,77240682,
		77240696,77240682, 77238127 ,77238128 ,77240379,77240380 };//只要是废卡过滤掉
		//泛用卡
		universal_codes = { 77239124,77240686,77240635, 77240625,77240619,77240547,77240546,77240080,77239976,77239921,77239634,77239575, 77239546,77239420,77239319,77239086,77238782,77238341,77238255,77238244,77238236,77238232,77238181,77238175,77238057,77238040,77238021,77238016,77238014,77238000,77238010,77238012,77238020,77238022,77238026,77238027,
		77238033,77238034,77238042,77238044,77238045,77238059,77238062,
		77238064,77238068,77238070,77238076,77238078,77238081,77238082,77238085,
		77238087,77238089,77238125,77238129,77238131,77238135,77238139,77238215,
		77238141,77238144,77238157,77238159,77238160,77238169,77238182,
		77238183,77238185,77238186,77238193,77238194,77238195,77238196,77238211,77238230,77238233,
		77238248,77238251,77238252,77238253,77238261,77238262,77238264,
		77238267,77238271,77238276,77238279,77238290,77238291,77238293,77238296,
		77238299,77238301,77238303,77238307,77238308,77238317,77238320,77238333,
		77238360,77238367,77238375,77238376,77238377,77238378,77238780,77238781,
		77238787,77238788,77238792,77238990,77238991,77239010,77239011,77239012,
		77239013,77239017,77239022,77239024,77239028,77239029,77239032,77239034,
		77239035,77239042,77239045,77239050,77239058,77239065,77239068,77239071,
		77239075,77239076,77239080,77239081,77239082,77239085,77239089,77239093,77239094,
		77239100,77239112,77239119,77239125,77239130,77239135,
		77239137,77239148,77239151,77239152,77239156,77239158,
			77239159,77239161,77239165,77239166,77239168,77239169,77239170,
			77239173,77239175,77239176,77239179,77239198,77239199,
			77239215,77239241,77239247,77239248,77239254,77239261,77239271,77239273,
			77239274,77239284,77239308,77239313,77239327,77239347,
			77239354,77239355,77239357,77239358,77239359,77239371,77239372,
			77239374,77239375,77239377,77239379,77239381,77239382,77239385,
			77239386,77239400,77239415,77239416,77239431,77239441,77239470,
			77239471,77239476,77239478,77239485,77239488,77239490,77239491,
			77239496,77239497,77239498,77239517,77239519,77239538,77239542,
			77239564,77239570,77239572,77239573,77239577,77239580,77239583,
			77239584,77239585,77239589,77239590,77239591,77239621,77239623,
			77239625,77239640,77239650,77239651,77239670,77239671,77239680,77239682,
			77239685,77239689,77239690,77239691,77239692,77239693,77239694,
			77239695,77239696,77239699,77239711,77239733,77239740,77239799,77239805,
			77239840,77239880,77239881,77239883,77239884,77239885,77239886,
			77239891,77239973,77239975,77239977,
			77240002,77240008,77240027,77240030,77240032,77240033,77240036,
			77240040,77240056,77240058,77240061,77240062,
			77240078,77240084,77240087,77240089,
			77240093,77240096,77240100,77240104,77240105,77240107,77240109,
			77240120,77240130,77240132,77240134,77240137,77240139,77240140,
			77240143,77240147,77240149,77240151,77240153,
			77240162,77240164,77240169,77240170,77240177,77240178,77240179,
			77240180,77240190,77240214,77240215,77240216,77240220,77240222,77240223,
			77240226,77240240,77240243,77240249,
			77240260,77240263,77240266,77240267,77240268,77240270,77240271,77240273,
			77240276,77240281,77240296,77240297,
			77240307,77240316,77240322,77240324,77240329,77240330,77240331,
			77240332,77240351,77240364,77240387,77240410,77240569,
			77240426,77240429,77240430,77240434,77240437,77240443,77240448,77240450,77240452,77240456,77240457,
			77240459,77240484,77240487,77240488,77240490,
			77240498,77240509,77240525,77240510,77240575,77240587,
			77240533,77240540,77240542,77240552,77240567,77240578,
			77240555,77240560,77240561,77240562,77240564,77240565,77240571,
			77240574,77240576,77240580,77240582,77240584,77240589,
			77240585,77240588,77240593,77240595,77240597,77240604,77240594,
			77240608,77240613,77240615,77240616,77240618,77240643,77240647,
			77240657,77240659,77240668,77240670,77240671,77240672,77240675,77240677,
			77240693,77240695,77240697,77240698,77240699,77240701,77239031,77239055,
			77239057,77239060,77239127,77239139,77239141,77240205,77240206,
			77240207,77240596

		};
		//限制泛用卡
		universal_codes_limit = { 77240583,77240287,77239475,77240161,77240065,77240229,77239581,77239105,77239104,77240066,77240065,77240064,77240144,77239174,77239036,77239118,77240055,77240073,77240074,77240075, 77238051,77239442,77239033,77239927,77240318,77239197,77240225,77240655,77240607,77240579,77240568,77240551,77240528,77240499,77240497, 77240462,77240454,77240435,77240432,77240586,77239916, 77239915,77238162,77239875,77240333,77238590,77240467,77240423,77240347,77240347,77240291,77240292, 77240256,77240208,77240166, 77240146,77240142,77240101,77240090,77240006,77239980,77239965,77239913,77239908,77239894,77239406,77239405,77239351,77239269,77239253,77239238,77239217,77239216,77239201,77239198,77239196,77239163,77239162,77239157,7239020,77238090,77238066,77238067,77238143,77238176,77238500,
		77238512,77239000,77239021,77239026,77239030,77239164, 77239167,
		77239206,77239208,77239231,77239237,
		77239282,77239290,77239291,77239299,77239300,77239301,77239317,
			77239376,77239395,77239477,77239582,77239588,77239686,
			77239750,77239760,77239930,77240198,
			77240199,77240217,77240218,77240272,77240275,77240444,
			77240471,77238188,77238212,77238204,77238165,77238166,77238177,
			77238213,77238214,77238500,77238794,77239001
		};
		//首个索引卡片是当前添加的卡片，次索引是与它被联动的卡片，如果卡号相同，则是同名系列卡，随机1-2张
		link_codes = { {77240690,77240690,77240690}, { 77240689,77240689,77240689 }, { 77239990,89943723,46986414,44508094 }, { 77239138,89631139,89631139 }, { 77239674,89631139,89631139 }, { 77239499,77239496,77239497,77239498 }, { 77239495,77240202,77240203,77240204 } ,{ 77239971,77239970,77239972 }, { 77240384,77240384 }, { 77240277,77240278,77240279 } ,{ 77240234,77239420 } ,{ 77240060,77240060 }, { 77240048,70095154,70095154,70095154 }, { 77240004,77239103,77239104,77239105 } ,{ 77239546,77239511 }, { 77239515,77239515 }, { 77239417,77239420 }, { 77239226,77239261 }, { 77239091,77239080,77239081,77239082,77239085 }
		,{ 77238410,77238291,77238292,77238293,77238294,77238295,77238296,77238297 }, { 77238328,77238328 }, { 77238254,77238254 },{ 77238005,33537328 },{77238006,46263076},{77238007,46263076},
			{77238008,10875327},{77238017,33537328},{77238018,15187079},{77238019,69931927},
			{77238023,10875327},{77238024,69931927},{77238029,77240487},{77238030,77239261,77239262,77239263},
			{77238031,77240681,77240680,77240679},{77238054,77238054,77238054},{77238080,77238080,77238080},
			{77238119,77238119,77238119},{77238126,77238127,77238128,77240379,77240380,77240682},{77238143,77239947},{77238158,77238157},{77238160,77238160,77238160},
			{77238202,77238180,77238181},{77238203,77238182},{77238204,77238188,77238192},
			{77238208,77238180,77238181},{77238209,77238194,77238195},{77238210,77238189},
			{77238212,77238188,77238211},{77238222,77238223},{77238223,77238222},{77238224,
		77238225,77238226},{77238263,89631139},{
		77238327,77240363,77240676},{77238782,74677422},{77238783,77238788,77238788,77238788},{77238792,74677422},
			{77238793,77238788},{77238795,77238789,77238789,77238789},{77238980,10000000},{77238981,10000020}
		,{77238982,10000010},{77238992,77239139},{77239001,77239000},{77239014,77239015,77239016,77239017,77239018,77239019},
			{77239050,77239047},{77239081,38033121},{77239087,38033121,80304126},{77239088,46986414},
			{77239100,10000000},{77239101,10000020},{77239102,10000010},{77239115,77239116,77239116},
			{77239121,77239120},{77239120,77239121},{77239123,77239120,77239122},{77239124,89631139},
			{77239127,89631139},{77239128,89631139,46986414},{77239134,89631139,89631139},{77239136,77239135},
			{77239143,77239133,77239133,77239142},{77239146,77239145,77239144,89631139},{77239160,77239160,77239160},
		{77239167,7902349,8124921,33396948,44519536,70903634},{77239174,24696097},{77239214,77239214,77239214},{77239228,77239228,77239228},
			{77239204,77239201},{77239201,77239204},{77239230,77239253},{77239236,77239235},{77239235,77239236},
			{77239239,77240472},{77239245,77239230},{77239253,77239230},{77239262,77239261},
			{77239263,77239262,77239261},{77239264,77239263,77239262,77239261},{77239265,
		77239264,77239263,77239262,77239261},{77239269,77239292},{77239284,77239295}
		,{77239290,77239231,77239232,77239233},{77239293,77239230},{77239295,77240024,77240076},
			{77239310,10000010,10000000,10000020},{77239317,77239302},{77239413,77239413,77239413},
			{77239432,77239406},{77239450,77239402},{77239496,77239491},{77239497,77238991},
			{77239498,77238990},{77239519,77239519,77239519},{77239522,77239523,77239524},
			{77239523,77239524},{77239531,77239511},{77239532,77239532,77239532},
			{77239543,77239506,77239507,77239510,77239509,77239505,77239508}, {77239586,77240003},
			{77239603,77239630,77239621},{77239610,77239611},{77239621,77239621,77239621},
			{77239626,77239625},{77239628,77240146},{77239629,77239611},{77239630,77239621},
			{77239631,77239629},{77239634,77239644},{77239635,77239644},{77239642,
		77239623,77239620},{77239644,77239621,77239605},{77239678,7902349,8124921,33396948,44519536,70903634
		},{77239799,77239400,77239400,77239400},{77239894,69890967,6007213,32491822,77239896,77239895,77239897},
			{77239905,10000010,10000000,10000020},{77239908,10000020},
			{77239910,10000010,10000000,10000020},
			{77239911,10000010,10000000,10000020,77239909},
			{77239912,10000010,10000000,10000020,77239900},
			{77239913,10000010,10000000,10000020,77239920,77239902},{77239915,10000010,10000000,10000020},
			{77239916,10000000,10000010},{77239921,10000010,10000000,10000020},
			{77239926,10000010,83764718},{77239930,10000010,10000000,10000020,77239931,
		77239932,77239933},{77239940,77239943},{77239941,77239944} ,{77239942,77239945},
			{77239960,77239978},{77239978,77239979},{77239979,77239960,40737112},
			{77239985,38033121,46986414},{77239991,77239992,77239993},{77239995,
		77239996,77239960,46986414},{77239997,77239995,77239996},{77239998,
		77239995,77239996,77239997},{77240000,89631139,77239121,77239146},
			{77240007,10000000,10000010,10000020,77239922},{77240073,69890967},{77240074,32491822},{77240075,6007213},
			{77240090,89631139},{77240142,77240142},{77240197,77240196,77240195},
			{77240198,77239909,77239931,77239932,77239933,77240200},{77240199,77239931,
		77239932,77239933,77240201},{77240205,77239491} ,{77240206,77238991},
		{77240207,77238990},{77240208,77240202,77240203,77240204},
			{77240283,77240280,77240280,77240280},{77240288,77240287},{77240287,77240288}, { 77240301,77240300 },
			{77240335,15259703},{77240339,53183600,90960358},{77240370,77240366,
		77240367,77240368,77240369},{77240371,77240364},{77240372,77240676},{77240373,77240361}
		,{77240383,55569674},{77240395,77240391,77240391,77240391},{77240396,77240396},
			{77240402,77240402,77240402},{77240419,77240396},{77240424,77240401},
			{77240436,77240433,77240433}, {77240453,77240442,77240433},{77240455,
		77240446,77240432,77240439},{77240459,77240478},{77240470,77239264},{77240474,77240475}
		,{77240478,77240479,77240480,77240481},{77240489,77240489,77240489},{77240491,77240491,77240491}
		,{77240492,77240492,77240492},{77240518,77240518,77240518},{77240525,77240525,77240525},
			{77240619,77240619,77240619},{77240630,77239236},{77240642,77240634,77240632},
			{77240647,77240647},{77240650,77240650,77240650},{77240653,77240652},{77240654,77240653,77240652,77240649},
			{77240657,77240658},{77240665,77240650},{77240315,77239400,77239400,77239400},{77238001}, };
		//针对卡，key为被针对的系列对象
		regarding_codes = { {ZCGSetCodeType::SStar,{77238058,77238060}},{ZCGSetCodeType::SoulBreaker,
			{77238065 }},{ZCGSetCodeType::Olekarks,{77239443,77240265}},{ZCGSetCodeType::Hades,{77240304}},
			{ZCGSetCodeType::Yaoge,{77240514,77240515,77240516}}, };
	}

	/// <summary>
	/// 卡组随机算法
	/// </summary>
	/// <returns></returns>
	std::vector<int> ZCGManager::GetRandomZCGDeckList() {
		int mainCountMin = 40;
		int mainCountMax = 120;
		int mainCount = GetRandomInt(mainCountMin, mainCountMax);//主卡组需要在40-120随机
		int exCountMin = 5;
		int exCountMAX = 30;
		int exCount = GetRandomInt(exCountMin, exCountMAX);//额外卡组需要在5-30随机
		//符合该系列的额外怪兽卡
		int exSetCodeMonsterCard = static_cast<int>(exCount / 10) * 4;
		//不符合该系列的额外怪兽卡
		int exNoneMonsterCard = exCount - exSetCodeMonsterCard;
		//获取到主卡组怪兽魔陷卡分布比
		ZCGContrast& card_type_contrast = card_type_contrasts[GetRandomInt(0, card_type_contrasts.size() - 1)];
		int mainMonsterCard = static_cast<int>(mainCount / 10) * card_type_contrast.leftKey;//规则上的怪兽卡数量
		int mainSpellCard = mainCount - mainMonsterCard;//规则上的怪兽卡数量
		//卡组的系列构筑类型
		ZCGSetCodeType deckCompositionType = static_cast<ZCGSetCodeType>(GetRandomInt(ZCGSetCodeType::Hades, ZCGSetCodeType::Cartoon));
		//获取到该系列构筑时系列卡和非系列卡的占比数量
		ZCGContrast& deck_composition_type_contrast = contrast_map[deckCompositionType];
		//符合该系列的怪兽卡
		int mainSetCodeMonsterCard = static_cast<int>(mainMonsterCard / 10) * deck_composition_type_contrast.leftKey;
		//不符合该系列的怪兽卡
		int mainNoneMonsterCard = mainMonsterCard - mainSetCodeMonsterCard;
		//符合该系列的魔陷卡
		int mainSetCodeSpellCard = static_cast<int>(mainSpellCard / 10) * deck_composition_type_contrast.leftKey;
		//不符合该系列的魔陷卡
		int mainNoneSpellCard = mainSpellCard - mainSetCodeSpellCard;
		//4星以及以下怪兽占比70%
		int mainLevel4BelowMonsterCard = static_cast<int>(mainMonsterCard / 10) * 7;
		//4星以上怪兽占比30%
		int mainLevel4UpMonsterCard = mainMonsterCard - mainLevel4BelowMonsterCard;
		std::vector<int> deck_list;
		//开始添加卡片，这里用拷贝之后的数组
		ZCGCodeGroup stype_group = stype_codes[deckCompositionType];
		ZCGCodeGroup _universal_group = universal_group;//泛用卡集合
		ZCGCodeGroup _universal_group_limit = universal_group_limit;//泛用卡限制分化集合
		ZCGCodeGroup _universal_setcode_group = universal_setcode_group;//泛用系列卡分化集合
		ZCGCodeGroup _ocodes = _codes;//存储剩余的随机卡片集合，也是一定程度上的泛用卡
		int MAX_LOOP = 6;
		//系列怪兽卡
		for (int i = 0; i < mainSetCodeMonsterCard; ++i) {
			BOOL isLevel4Below = TRUE;
			if (mainLevel4BelowMonsterCard <= 0) {
				isLevel4Below = FALSE;
			}
			else if (mainLevel4UpMonsterCard <= 0) {
				isLevel4Below = TRUE;
			}
			else {
				isLevel4Below = GetRandomInt(0, 1);
			}
			std::vector<unsigned int>* pmonsterCodes = nullptr;
			pmonsterCodes = isLevel4Below ? &stype_group.main4BelowMonsterCodes : &stype_group.main4UpMonsterCodes;
			//如果整个系列没有怪兽卡，则从泛用卡里面抽取
			if (pmonsterCodes->size() <= 0) {
				pmonsterCodes = isLevel4Below ? &_universal_group.main4BelowMonsterCodes : &_universal_group.main4UpMonsterCodes;
			}
			if (pmonsterCodes->size() <= 0) {
				pmonsterCodes = isLevel4Below ? &_universal_setcode_group.main4BelowMonsterCodes : &_universal_setcode_group.main4UpMonsterCodes;
			}
			unsigned int code = 0;
			int maxTimes = -1;//最大随机循环次数
			while (maxTimes < MAX_LOOP) {
				maxTimes++;
				code = (*pmonsterCodes)[GetRandomInt(0, pmonsterCodes->size() - 1)];
				if (GetVecElementCount(deck_list, code) >= GetDeckMaxCount(code)) continue;//如果卡片大于卡组中所要的最大值则不添加
				break;
			}
			if (isLevel4Below) {
				mainLevel4BelowMonsterCard -= 1;
			}
			else {
				mainLevel4UpMonsterCard -= 1;
			}
			deck_list.push_back(code);
			//对于添加之后的卡片如果超出限制则移除容器中原来的卡片
			if (GetVecElementCount(deck_list, code) >= GetDeckMaxCount(code)) RemoveCode(*pmonsterCodes, code);
			//检查联动卡
			AddLinkCode(deck_list, code);
		}
		//系列魔陷卡
		for (int i = 0; i < mainSetCodeSpellCard; ++i) {
			std::vector<unsigned int>* pspellrCodes = nullptr;
			pspellrCodes = &stype_group.mainSpellCodes;
			if (pspellrCodes->size() <= 0) pspellrCodes = &_universal_group.mainSpellCodes;
			if (pspellrCodes->size() <= 0) pspellrCodes = &_universal_setcode_group.mainSpellCodes;
			unsigned int code = 0;
			int maxTimes = -1;//最大随机循环次数
			while (maxTimes < MAX_LOOP) {
				maxTimes++;
				code = (*pspellrCodes)[GetRandomInt(0, pspellrCodes->size() - 1)];
				if (GetVecElementCount(deck_list, code) >= GetDeckMaxCount(code)) continue;//如果卡片大于3张不添加
				break;
			}
			deck_list.push_back(code);
			if (GetVecElementCount(deck_list, code) >= GetDeckMaxCount(code)) RemoveCode(*pspellrCodes, code);
			AddLinkCode(deck_list, code);
		}
		//非系列怪兽卡
		for (int i = 0; i < mainNoneMonsterCard; ++i) {
			BOOL isLevel4Below = TRUE;
			if (mainLevel4BelowMonsterCard <= 0) {
				isLevel4Below = FALSE;
			}
			else if (mainLevel4UpMonsterCard <= 0) {
				isLevel4Below = TRUE;
			}
			else {
				isLevel4Below = GetRandomInt(0, 1);
			}
			std::vector<unsigned int>* pmonsterCodes = nullptr;
			int indexValue = GetRandomInt(0, 100);
			if (indexValue <= 60) {
				pmonsterCodes = isLevel4Below ? &_universal_group.main4BelowMonsterCodes : &_universal_group.main4UpMonsterCodes;
			}
			else if (indexValue <= 85) {
				pmonsterCodes = isLevel4Below ? &_universal_setcode_group.main4BelowMonsterCodes : &_universal_setcode_group.main4UpMonsterCodes;
			}
			else if (indexValue <= 99) {
				pmonsterCodes = isLevel4Below ? &_ocodes.main4BelowMonsterCodes : &_ocodes.main4UpMonsterCodes;
			}
			else {
				pmonsterCodes = isLevel4Below ? &_universal_group_limit.main4BelowMonsterCodes : &_universal_group_limit.main4UpMonsterCodes;
			}
			if (pmonsterCodes->size() <= 0) pmonsterCodes = isLevel4Below ? &_universal_setcode_group.main4BelowMonsterCodes : &_universal_setcode_group.main4UpMonsterCodes;
			unsigned int code = 0;
			int maxTimes = -1;//最大随机循环次数
			while (maxTimes < MAX_LOOP) {
				maxTimes++;
				code = (*pmonsterCodes)[GetRandomInt(0, pmonsterCodes->size() - 1)];
				if (GetVecElementCount(deck_list, code) >= GetDeckMaxCount(code)) continue;//如果卡片大于3张不添加
				break;
			}
			if (isLevel4Below) {
				mainLevel4BelowMonsterCard -= 1;
			}
			else {
				mainLevel4UpMonsterCard -= 1;
			}
			deck_list.push_back(code);
			if (GetVecElementCount(deck_list, code) >= GetDeckMaxCount(code)) RemoveCode(*pmonsterCodes, code);
			AddLinkCode(deck_list, code);
		}
		//非系列魔法卡
		for (int i = 0; i < mainNoneSpellCard; ++i) {
			std::vector<unsigned int>* pspellCodes = nullptr;
			int indexValue = GetRandomInt(0, 100);
			if (indexValue <= 60) {
				pspellCodes = &_universal_group.mainSpellCodes;
			}
			else if (indexValue <= 85) {
				pspellCodes = &_universal_setcode_group.mainSpellCodes;
			}
			else if (indexValue <= 99) {
				pspellCodes = &_ocodes.mainSpellCodes;
			}
			else {
				pspellCodes = &_universal_group_limit.mainSpellCodes;
			}
			if (pspellCodes->size() <= 0) pspellCodes = &_universal_setcode_group.mainSpellCodes;
			unsigned int code = 0;
			int maxTimes = -1;//最大随机循环次数
			while (maxTimes < MAX_LOOP) {
				maxTimes++;
				code = (*pspellCodes)[GetRandomInt(0, pspellCodes->size() - 1)];
				if (GetVecElementCount(deck_list, code) >= GetDeckMaxCount(code)) continue;//如果卡片大于3张不添加
				break;
			}
			deck_list.push_back(code);
			if (GetVecElementCount(deck_list, code) >= GetDeckMaxCount(code)) RemoveCode(*pspellCodes, code);
			AddLinkCode(deck_list, code);
		}
		bool has_type_fusion = false;
		//系列额外怪兽
		auto _pdatas = &dataManager._datas;
		std::vector<unsigned int> ecodes = { 77239149,77240315 };//77239175 77239693 77239799 77239695
		/*for (int i = 0; i < ecodes.size(); ++i) {
			RemoveCode(stype_group.extraMonsterCodes, ecodes[i]);
			RemoveCode(_ocodes.extraMonsterCodes, ecodes[i]);
		}*/
	/*	ecodes.clear();*/
		for (int i = 0; i < exSetCodeMonsterCard; ++i) {
			//先从系列额外中抽取指定的额外怪兽卡
			std::vector<unsigned int>* pmonsterCodes = &stype_group.extraMonsterCodes;
			if (pmonsterCodes->size() <= 0) pmonsterCodes = &_ocodes.extraMonsterCodes;	//额外卡中没有禁止卡和限制卡
			int maxTimes = -1;//最大随机循环次数
			unsigned int code = 0;
			while (maxTimes < MAX_LOOP) {
				maxTimes++;
				int randomFilter = ecodes.size() > 0 ? GetRandomInt(0, 100) : 0;
				if (randomFilter < 95) {
					code = (*pmonsterCodes)[GetRandomInt(0, pmonsterCodes->size() - 1)];
				}
				else {
					code = ecodes[GetRandomInt(0, ecodes.size() - 1)];
					ecodes.clear();
				}
				if (GetVecElementCount(deck_list, code) >= 3) continue;//如果卡片大于3张不添加
				break;
			}
			if (!has_type_fusion) {
				if (_pdatas && _pdatas->find(code) != _pdatas->end()) {
					CardDataC& cd = (*_pdatas)[code];
					if (cd.type & TYPE_FUSION)  has_type_fusion = true;
				}
			}
			deck_list.push_back(code);
			//随机一个值移除
			if (GetVecElementCount(deck_list, code) >= GetRandomInt(1, 3)) {
				RemoveCode(stype_group.extraMonsterCodes, code);
				RemoveCode(_ocodes.extraMonsterCodes, code);
			}
			AddLinkCode(deck_list, code);
		}
		//非系列额外怪兽
		for (int i = 0; i < exNoneMonsterCard; ++i) {
			std::vector<unsigned int>* pmonsterCodes = &_ocodes.extraMonsterCodes;
			if (pmonsterCodes->size() <= 0) break;
			int maxTimes = -1;//最大随机循环次数
			unsigned int code = 0;
			while (maxTimes < MAX_LOOP) {
				maxTimes++;
				int randomFilter = ecodes.size() > 0 ? GetRandomInt(0, 100) : 0;
				if (randomFilter < 95) {
					code = (*pmonsterCodes)[GetRandomInt(0, pmonsterCodes->size() - 1)];
				}
				else {
					code = ecodes[GetRandomInt(0, ecodes.size() - 1)];
					ecodes.clear();
				}
				if (GetVecElementCount(deck_list, code) >= 3) continue;//如果卡片大于3张不添加
				break;
			}
			if (!has_type_fusion) {
				if (_pdatas && _pdatas->find(code) != _pdatas->end()) {
					CardDataC& cd = (*_pdatas)[code];
					if (cd.type & TYPE_FUSION)  has_type_fusion = true;
				}
			}
			deck_list.push_back(code);
			//随机一个值移除
			if (GetVecElementCount(deck_list, code) >= GetRandomInt(1, 3)) {
				RemoveCode(_ocodes.extraMonsterCodes, code);
			}
			AddLinkCode(deck_list, code);
		}
		if (has_type_fusion) {
			std::vector<unsigned int> fcodes = { 77238220 ,77238221 ,77239641 };
			int fcount = GetVecElementCount(deck_list, 77238220) +
				GetVecElementCount(deck_list, 77238221) + GetVecElementCount(deck_list, 77239641);
			int keyCount = GetRandomInt(1, 3);
			while (fcount < keyCount) {
				int code = fcodes[GetRandomInt(0, fcodes.size() - 1)];
				deck_list.push_back(code);
				++fcount;
			}
		}
		//添加规则重置卡
		deck_list.push_back(99997999);
		return deck_list;
	}

	std::vector<unsigned int> ZCGManager::FindSetCodesByValue(unsigned int target_value) {
		std::vector<unsigned int> keys;  // 用于存储符合条件的 key
		for (const auto& pair : setcode_map) {
			if (pair.second == target_value) {  // 检查 value 是否满足条件
				keys.push_back(pair.first);  // 将满足条件的 key 添加到结果中
			}
		}
		return keys;
	}

	std::vector<CardDataC*> ZCGManager::CodesToCardsC(const std::vector<unsigned int>& codes) {
		std::vector<CardDataC*> cards;
		auto& _datas = dataManager._datas;
		for (int i = 0; i < codes.size(); ++i) {
			if (_datas.find(codes[i]) != _datas.end()) {
				cards.push_back(&_datas[codes[i]]);
			}
		}
		return cards;
	}
	std::vector<CardDataC*> ZCGManager::CodesToCardsC(const std::vector<int>& codes) {
		std::vector<CardDataC*> cards;
		auto& _datas = dataManager._datas;
		for (int i = 0; i < codes.size(); ++i) {
			if (_datas.find(static_cast<unsigned int>(codes[i])) != _datas.end()) {
				cards.push_back(&_datas[static_cast<unsigned int>(codes[i])]);
			}
		}
		return cards;
	}
	void ZCGManager::InitDataGroup() {
		auto& _datas = dataManager._datas;
		for (int i = 0; i < zcgManager.universal_codes.size(); ++i) {
			unsigned int code = zcgManager.universal_codes[i];
			if (_datas.find(code) != _datas.end()) {//包含指定的key
				CardDataC& cd = _datas[code];
				if (cd.type & TYPES_EXTRA_DECK) {//添加到额外卡组的卡片
					zcgManager.universal_group.extraMonsterCodes.push_back(code);
				}
				else if (cd.type & (TYPE_SPELL | TYPE_TRAP)) {//添加魔陷
					zcgManager.universal_group.mainSpellCodes.push_back(code);
				}
				else if (cd.type & TYPE_MONSTER) {
					cd.level > 4 ? zcgManager.universal_group.main4UpMonsterCodes.push_back(code) ://添加等级4以上
						zcgManager.universal_group.main4BelowMonsterCodes.push_back(code);//添加等级4以下
				}
			}
		}
		for (int i = 0; i < zcgManager.universal_codes_limit.size(); ++i) {
			unsigned int code = zcgManager.universal_codes_limit[i];
			if (_datas.find(code) != _datas.end()) {//包含指定的key
				CardDataC& cd = _datas[code];
				if (cd.type & TYPES_EXTRA_DECK) {//添加到额外卡组的卡片
					zcgManager.universal_group_limit.extraMonsterCodes.push_back(code);
				}
				else if (cd.type & (TYPE_SPELL | TYPE_TRAP)) {//添加魔陷
					zcgManager.universal_group_limit.mainSpellCodes.push_back(code);
				}
				else if (cd.type & TYPE_MONSTER) {
					cd.level > 4 ? zcgManager.universal_group_limit.main4UpMonsterCodes.push_back(code) ://添加等级4以上
						zcgManager.universal_group_limit.main4BelowMonsterCodes.push_back(code);//添加等级4以下
				}
			}
		}
		for (int i = 0; i < zcgManager.universal_codes_setcode.size(); ++i) {
			unsigned int code = zcgManager.universal_codes_setcode[i];
			if (_datas.find(code) != _datas.end()) {//包含指定的key
				CardDataC& cd = _datas[code];
				if (cd.type & TYPES_EXTRA_DECK) {//添加到额外卡组的卡片
					zcgManager.universal_setcode_group.extraMonsterCodes.push_back(code);
				}
				else if (cd.type & (TYPE_SPELL | TYPE_TRAP)) {//添加魔陷
					zcgManager.universal_setcode_group.mainSpellCodes.push_back(code);
				}
				else if (cd.type & TYPE_MONSTER) {
					cd.level > 4 ? zcgManager.universal_setcode_group.main4UpMonsterCodes.push_back(code) ://添加等级4以上
						zcgManager.universal_setcode_group.main4BelowMonsterCodes.push_back(code);//添加等级4以下
				}
			}
		}
	}

	int ZCGManager::GetRandomInt(int min, int max) {
		std::uniform_int_distribution<int> dis(min, max);
		return dis(gen);
	}
	int ZCGManager::GetVecElementCount(const std::vector<unsigned int>& vec, int element) {
		int count = 0;
		for (unsigned int num : vec) {
			if (num == element) {
				count++;
			}
		}
		return count;
	}
	/// <summary>
	/// 获取这张卡在卡组最大应该限制的卡片数量
	/// </summary>
	/// <param name="code"></param>
	/// <returns></returns>
	int ZCGManager::GetDeckMaxCount(unsigned int code) {
		int limitCount = 3;
		//	GetVecElementCount(filter_limit_one_codes, code) > 0 ?
		limitCount = GetVecElementCount(filter_limit_zero_codes, code) > 0 ? 0 :
			GetVecElementCount(filter_limit_two_codes, code) > 0 ? 2 : GetVecElementCount(filter_limit_three_codes, code) > 0 ? 3
			: 1;
		return limitCount;
	}
	void ZCGManager::RemoveCode(std::vector<unsigned int>& codes, unsigned int code) {
		if (codes.size() <= 0) return;
		// 使用 std::remove 将所有匹配元素移到末尾，并返回第一个不匹配元素的迭代器
		auto newEnd = std::remove(codes.begin(), codes.end(), code);
		// 使用 erase 移除所有匹配的元素
		codes.erase(newEnd, codes.end());
	}
	int ZCGManager::GetVecElementCount(const std::vector<int>& vec, int element) {
		int count = 0;
		for (int num : vec) {
			if (num == element) {
				count++;
			}
		}
		return count;
	}
	/// <summary>
	/// 合并2个容器，返回新容器
	/// </summary>
	/// <param name="a"></param>
	/// <param name="b"></param>
	/// <returns></returns>
	std::vector<unsigned int> ZCGManager::MergeVec(const std::vector<unsigned int>& a, const std::vector<unsigned int>& b) {
		std::vector<unsigned int> result(a);
		result.insert(result.end(), b.begin(), b.end());
		return result;
	}
	/// <summary>
	/// 添加联动卡
	/// </summary>
	/// <param name="deck_list"></param>
	/// <param name="code"></param>
	void ZCGManager::AddLinkCode(std::vector<int>& deck_list, unsigned int code) {
		if (AddSpecialLinkCode(deck_list, code)) return;//特殊联动直接返回
		for (int i = 0; i < link_codes.size(); ++i) {
			if (code == link_codes[i][0]) { //添加联动卡
				int lastCode = code;
				for (int j = 1; j < link_codes[i].size(); ++j) {
					int linkCode = link_codes[i][j];
					if ((linkCode == lastCode) && GetVecElementCount(deck_list, linkCode) < 3) {//同名卡不检查直接添加
						deck_list.push_back(linkCode);
					}
					else {
						if (GetVecElementCount(deck_list, linkCode) < GetDeckMaxCount(linkCode)) {
							deck_list.push_back(linkCode);
							AddLinkCode(deck_list, linkCode); //递归添加
						}
					}
					lastCode = linkCode;
				}
			}
		}
	}
	bool ZCGManager::AddCode(std::vector<int>& deck_list, const std::vector<CardDataC*>& cards, int randomSize) {
		bool result = false;
		if (cards.size() <= 0) return result;
		for (int i = 0; i < randomSize; ++i) {
			//获取到随机到的卡片
			CardDataC* pcard = cards[GetRandomInt(0, cards.size() - 1)];
			int deckCount = GetVecElementCount(deck_list, pcard->code);//当前卡组中这张卡的数量
			int maxCount = GetDeckMaxCount(pcard->code);//返回卡组中这个卡号的卡允许存在的最大数量
			if (deckCount >= maxCount) continue;//如果当前卡组这个卡号的卡片已经达到最大值，不允许添加
			deck_list.push_back(pcard->code);
			result = true;
			AddLinkCode(deck_list, pcard->code);//递归添加
		}
		return result;
	}
	bool ZCGManager::AddCode(std::vector<int>& deck_list, const std::vector<unsigned int>& codes, int randomSize, bool maxfilter) {
		bool result = false;
		if (codes.size() <= 0) return result;
		for (int i = 0; i < randomSize; ++i) {
			//获取到随机到的卡片
			unsigned int ncode = codes[GetRandomInt(0, codes.size() - 1)];
			int deckCount = GetVecElementCount(deck_list, ncode);//当前卡组中这张卡的数量
			int maxCount = maxfilter ? GetDeckMaxCount(ncode) :
				GetVecElementCount(filter_limit_zero_codes, ncode) > 0 ? 0 :3;//返回卡组中这个卡号的卡允许存在的最大数量
			if (deckCount >= maxCount) continue;//如果当前卡组这个卡号的卡片已经达到最大值，不允许添加
			deck_list.push_back(ncode);
			result = true;
			AddLinkCode(deck_list, ncode);//递归添加
		}
		return result;
	}
	/// <summary>
	///新置算法添加特殊的卡片，不再随着map来增加卡号
	/// </summary>
	/// <param name="deck_list"></param>
	/// <param name="code"></param>
	/// <returns></returns>
	bool ZCGManager::AddSpecialLinkCode(std::vector<int>& deck_list, unsigned int code) {
		auto& AddXYZCount = [this](std::vector<int>& deck_list,
			unsigned int code, int level, int count, bool& maxfilter, int& randomSize,
			std::vector<unsigned int>& codes)->bool {
				int mcount = CheckCardsCount(deck_list, [this, level](const CardDataC& card)->bool {
					return (card.type & (TYPE_MONSTER)) && card.level == level;
					}, code);
				if (mcount >= 3) return false;//卡组中有9星怪兽3只则不添加
				maxfilter = false;
				randomSize = 3 - mcount;
				codes = FilterCardsCodes([this, level](const CardDataC& card)->bool {
					return (card.type & (TYPE_MONSTER)) && card.level == level;
					});
				return true;
			};
		std::vector<unsigned int> _codes = { code };
		std::vector<CardDataC*> ccards = CodesToCardsC(_codes);
		CardDataC* pcard = nullptr;
		if (ccards.size() >= 0) pcard = ccards[0];
		auto& _datas = dataManager._datas;
		bool result = false;
		std::vector<unsigned int> codes;
		int randomSize = 1;//随机添加1到3张卡
		bool originalLink = false;//是否继续遍历
		bool maxfilter = true;
		if (pcard != nullptr && (pcard->type & TYPE_TOON)) { //是卡通怪兽检查添加卡通世界
			if (!HasCode(deck_list, { 77240346 },code)) deck_list.push_back(77240346);
		}
		//地缚神系列，额外添加带有地缚神字段的怪兽1~3张
		if (code == 77238001 || code == 77238002 || code == 77238009 || code == 77238014
			|| code == 77238015 || code == 77238021 || code == 77238025 || code == 77238228
			|| code == 77238236 || code == 77239994 || code == 77240694) {
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcodes(FindSetCodesByValue(ZCGSetCodeType::GodEarth))
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				return card.is_setcodes(FindSetCodesByValue(ZCGSetCodeType::GodEarth))
					&& (card.type & TYPE_MONSTER);
				});
			//auto& group = stype_codes[ZCGSetCodeType::GodEarth];
			////获取到所有的怪兽卡
			//codes = MergeVec(group.main4UpMonsterCodes, group.main4BelowMonsterCodes);
		}
		//R9超量*3
		else if (code == 77238072 || code == 77238075) {
			if (!AddXYZCount(deck_list, code, 9, 3, maxfilter, randomSize,
				codes)) return false;
		}
		//R8超量*3
		else if (code == 77239690 || code == 77238074 || code == 77239693) {
			if (!AddXYZCount(deck_list, code, 8, 3, maxfilter, randomSize,
				codes)) return false;
		}
		//R8超量*2
		else if (code == 77239690 || code == 77239695 || code == 77239885
			|| code == 77240058) {
			if (!AddXYZCount(deck_list, code, 8, 2, maxfilter, randomSize,
				codes)) return false;
		}
		//R10超量*3
		else if (code == 77239691 || code == 77240052) {
			if (!AddXYZCount(deck_list, code, 10, 3, maxfilter, randomSize,
				codes)) return false;
		}
		//R4超量*3
		else if (code == 77239692 || code == 77239886 || code == 77240659) {
			if (!AddXYZCount(deck_list, code, 4, 3, maxfilter, randomSize,
				codes)) return false;
		}
		//R2超量*2
		else if (code == 77239699) {
			if (!AddXYZCount(deck_list, code, 2, 2, maxfilter, randomSize,
				codes)) return false;
		}
		//R4超量*2
		else if (code == 77238233 || code == 77238234 || code == 77238780
			|| code == 77239130 || code == 77239175 || code == 77240660) {
			if (!AddXYZCount(deck_list, code, 4, 2, maxfilter, randomSize,
				codes)) return false;
		}
		//R5超量*3
		else if (code == 77238781 || code == 77239805) {
			if (!AddXYZCount(deck_list, code, 5, 3, maxfilter, randomSize,
				codes)) return false;
		}
		//R5超量*2
		else if (code == 77239689 || code == 77239887 || code == 77240286) {
			if (!AddXYZCount(deck_list, code, 5, 2, maxfilter, randomSize,
				codes)) return false;
		}
		//R12超量*2
		else if (code == 77239400) {
			if (!AddXYZCount(deck_list, code, 12, 2, maxfilter, randomSize,
				codes)) return false;
		}
		//R6超量*2
		else if (code == 77239589) {
			if (!AddXYZCount(deck_list, code, 6, 2, maxfilter, randomSize,
				codes)) return false;
		}
		//R7超量*2
		else if (code == 77240145) {
			if (!AddXYZCount(deck_list, code, 7, 2, maxfilter, randomSize,
				codes)) return false;
		}
		//R3超量*3
		else if (code == 77240088) {
			if (!AddXYZCount(deck_list, code, 3, 3, maxfilter, randomSize,
				codes)) return false;
		}
		//is_setcodes怪兽
		else if (code == 77238077 || code == 77240388) {
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcodes(FindSetCodesByValue(ZCGSetCodeType::Ancient))
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			auto& group = stype_codes[ZCGSetCodeType::Ancient];
			codes = MergeVec(group.main4UpMonsterCodes, group.main4BelowMonsterCodes);
		}
		else if (code == 77240340) { //卡通
			int size = CheckCardsCount(deck_list, [this](const CardDataC& card)->bool {
				return card.type & TYPE_TOON;
				}, code);
			if (size >= 3) return false;
			randomSize = 3 - size;
			maxfilter = false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.type & TYPE_TOON;
			});
		}
		else if (code == 77240496) {
			int size = CheckCardsCount(deck_list, [this](const CardDataC& card)->bool {
				return !(card.type & TYPE_XYZ) && (card.race & RACE_INSECT)
					&& card.level == 3;
				}, code);
			if (size >= 2) return false;
			randomSize = 2 - size;
			maxfilter = false;
			originalLink = true;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return !(card.type & TYPE_XYZ) && (card.race & RACE_INSECT) && card.level == 3;
				});
		}
		else if (code == 77240437) { //7星昆虫族
			int size = CheckCardsCount(deck_list, [this](const CardDataC& card)->bool {
				return !(card.type & TYPE_XYZ) && (card.race & RACE_INSECT)
					&& card.level == 7;
				}, code);
			if (size >= 2) return false;
			randomSize = 2 - size;
			maxfilter = false;
			originalLink = true;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return !(card.type & TYPE_XYZ) && (card.race & RACE_INSECT) && card.level == 7;
				});
		}
		else if (code == 77240436) {//4星昆虫族
			int size = CheckCardsCount(deck_list, [this](const CardDataC& card)->bool {
				return !(card.type & TYPE_XYZ)&& (card.race & RACE_INSECT)
					&& card.level == 4;
				}, code);
			if (size >= 2) return false;
			randomSize = 2 - size;
			maxfilter = false;
			originalLink = true;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return !(card.type & TYPE_XYZ) && (card.race & RACE_INSECT) && card.level == 4;
			});
		}
		else if (code == 77239602) {
			int size = CheckCardsCount(deck_list, [this](const CardDataC& card)->bool {
				return (card.race & RACE_PLANT);
				}, code);
			if (size >= 5) return false;
			randomSize = 5 - size;
			maxfilter = false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return (card.race & RACE_PLANT);
			});
		}
		else if (code == 77240308 || code == 77240309 || code == 77240310
			|| code == 77240312 || code == 77240313) {//冥界
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0xa13)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0xa13) && (card.type & TYPE_MONSTER);
				});
		}
		//同调怪兽
		else if (pcard && (pcard->type & TYPE_SYNCHRO)) {
			int pcode = pcard->code;
			if (pcode == 77239971 || pcode == 77240335) return false;
			maxfilter = false;
			if (!CheckCardsZero(deck_list, [pcode, this](const CardDataC& card)->bool {
				if (pcode == 77239605 || pcode == 77239606 //植物愤怒
					|| pcode == 77239607) return card.is_setcode(0xa90) && (card.type & TYPE_TUNER);
				if (pcode == 77239882) return (card.attribute & ATTRIBUTE_LIGHT) && (card.type & TYPE_TUNER);
				if (pcode == 77240661 || pcode == 77240662)return card.is_setcode(0xa33) && (card.type & TYPE_TUNER);
				return (card.type & TYPE_TUNER);
				}, code)) return false;
			randomSize = GetRandomInt(2, 3);
			codes = FilterCardsCodes([pcode, this](const CardDataC& card)->bool {
				if (pcode == 77239605 || pcode == 77239606 //植物愤怒
					|| pcode == 77239607) return card.is_setcode(0xa90) && (card.type & TYPE_TUNER);
				if (pcode == 77239882) return (card.attribute & ATTRIBUTE_LIGHT) && (card.type & TYPE_TUNER);
				if (pcode == 77240661 || pcode == 77240662)return card.is_setcode(0xa33) && (card.type & TYPE_TUNER);
				return (card.type & TYPE_TUNER);
				});
		}
		else if (code == 77239835 || code == 77239838 || code == 77239860 ||
			code == 77240269 || code == 77240289 || code == 77240293 || code == 77240298
			|| code == 77240300) { //装甲
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0xa110)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0xa110) && (card.type & TYPE_MONSTER);
				});
		}
		//反转怪兽
		else if (code == 77240043) {
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return (card.type & TYPE_FLIP);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return (card.type & TYPE_FLIP);
				});
		}
		//奥西里斯
		else if (code == 77238079 || code == 77239725 || code == 77240026 ||
			code == 77240239 || code == 77240256) {
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcodes(FindSetCodesByValue(ZCGSetCodeType::Osiris))
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			auto& group = stype_codes[ZCGSetCodeType::Osiris];
			codes = MergeVec(group.main4UpMonsterCodes, group.main4BelowMonsterCodes);
		}
		//不死族怪兽
		else if (code == 77238100 || code == 77238102 || code == 77238103 ||
			code == 77240382 || code == 77240670) {
			if (code == 77238100 || code == 77238102 || code == 77238103) randomSize = 3;
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return (card.race & RACE_ZOMBIE)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.race & RACE_ZOMBIE;
				});
		}
		else if (code == 77238182) { //2500以下战士族
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.defense <= 2500
					&& (card.race & RACE_WARRIOR);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return  card.defense <= 2500 && (card.race & RACE_WARRIOR);
				});
		}
		else if (code == 77240055) {//三卡通幻魔
			codes = { 77240073,77240074,77240075 };
			if (HasCode(deck_list, codes, code)) return false;
		}
		else if (code == 77239910) { //三幻神之光
			originalLink = true;
			codes = { 77239900,77239903,77239904 };
			if (HasCode(deck_list, codes, code)) return false;
		}
		else if (code == 77240063 || code == 77240116 || code == 77240119
			|| code == 77240121 || code == 77240124 || code == 77240129) { //天龙.机甲天龙
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				if (code == 77240063) return card.is_setcodes({ 0xdb98, 0xdb97 })
					&& (card.type & TYPE_MONSTER);
				return card.is_setcodes({ 0xdb97 })
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				if (code == 77240063) return card.is_setcodes({ 0xdb98, 0xdb97 })
					&& (card.type & TYPE_MONSTER);
				return card.is_setcodes({ 0xdb97 })
					&& (card.type & TYPE_MONSTER);
				});
		}
		//卡通
		else if (code == 77240346 || code == 77240347) {
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				return card.is_setcodes({ 0x62 })
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				return card.is_setcodes({ 0x62 })
					&& (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77239621) { //古树，等级2
			originalLink = true;
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				return card.level == 2
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				return card.level == 2
					&& (card.type & TYPE_MONSTER);
				});
		}
		//武藤游戏
		else if (code == 77238231 || code == 77238232 || code == 77238235) {
			codes = { 77240134,77238231,77238232,77238235 };
			if (HasCode(deck_list, codes, code)) return false;
		}
		else if (code == 77239118) { //神石板
			codes = { 77239106,77239107,77239108 };
			if (HasCode(deck_list, codes, code)) return false;
		}
		else if (code == 77238322 || code == 77238323 || code == 77238328 || code == 77238341
			|| code == 77238362 || code == 77238365 ||
			code == 77238366 || code == 77238368 || code == 77240111 || code == 77240112
			|| code == 77240113 || code == 77240114 || code == 77240115 || code == 77240181
			|| code == 77240317 || code == 77240318 || code == 77240319) { //太阳神
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				if (code == 77238323) return card.is_setcode(0xa210)
					&& (card.type & (TYPE_SPELL | TYPE_TRAP));
				return card.is_setcode(0xa210)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				if (code == 77238323) return card.is_setcode(0xa210)
					&& (card.type & (TYPE_SPELL | TYPE_TRAP));
				return card.is_setcode(0xa210) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77238326) { //不死
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0xa250);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0xa250);
				});
		}
		else if (code == 77240447) { //原始昆虫
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0xa130)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0xa130) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77240427 || code == 77240428 || code == 77240431) { //上古
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0xa120)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0xa120) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77238500 || code == 77240544 || code == 77240569) { //欧贝利斯克
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0xa220)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0xa220) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77239415) { //12星
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.level == 12
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.level == 12 && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77239504 || code == 77239518 ||
			code == 77239521 || code == 77239529 || code == 77239530 ||
			code == 77239532 || code == 77239536 || code == 77239540 ||
			code == 77239544 || code == 77239545 || code == 77239563) { //女子佣兵
			if (code == 77239532) originalLink = true;
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				return card.is_setcode(code == 77239540 ? 0xa81 : 0xa80)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				return card.is_setcode(code == 77239540 ? 0xa81 : 0xa80) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77239444 || code == 77240236) { //六芒星之龙
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0xa71)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0xa71) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77239414 || code == 77239416 || code == 77239418
			|| code == 77239419 || code == 77239430 || code == 77239440 ||
			code == 77240235 || code == 77240237) {//六芒星
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				if (code == 77239418)return card.is_setcode(0xa70);
				return card.is_setcode(0xa70)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				if (code == 77239418)return card.is_setcode(0xa70);
				return card.is_setcode(0xa70) && (card.type & TYPE_MONSTER);
				});

		}
		else if (code == 77239401 || code == 77239402 || code == 77239403 ||
			code == 77239404 || code == 77239406 || code == 77239405 ||
			code == 77240238) { //六芒星龙
			int attribute = 0;
			if (code == 77239401) attribute = ATTRIBUTE_EARTH;
			if (code == 77239402) attribute = ATTRIBUTE_WIND;
			if (code == 77239403 || code == 77240238) attribute = ATTRIBUTE_DARK;
			if (code == 77239404) attribute = ATTRIBUTE_LIGHT;
			if (code == 77239405) attribute = ATTRIBUTE_FIRE;
			if (code == 77239406) attribute = ATTRIBUTE_WATER;
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return (card.type & (TYPE_MONSTER)) && card.is_setcode(0xa70);
				}, code) || CheckCardsCount(deck_list, [attribute, this](const CardDataC& card)->bool {
					return (card.attribute & attribute);
					}, code) >= 2) return false;
				bool one = GetRandomInt(0, 1) == 1 ? true : false;
				if (one) {
					codes = FilterCardsCodes([attribute, this](const CardDataC& card)->bool {
						return (card.attribute & attribute);
						});
				}
				else {
					randomSize = 2;
					codes = FilterCardsCodes([this](const CardDataC& card)->bool {
						return  (card.type & (TYPE_MONSTER)) && card.is_setcode(0xa70);
						});
				}
		}
		else if (code == 77239239) { //仪式魔法
			originalLink = true;
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return (card.type & (TYPE_SPELL | TYPE_RITUAL));
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return (card.type & (TYPE_SPELL | TYPE_RITUAL));
				});
		}
		else if (code == 77239203 || code == 77239207 || code == 77239210 ||
			code == 77239218 || code == 77239220 || code == 77239221
			|| code == 77239222 || code == 77239223 || code == 77239225
			|| code == 77239227 || code == 77239229 || code == 77239237 ||
			code == 77239240 || code == 77239242 || code == 77239243
			|| code == 77239244 || code == 77239245 || code == 77239249
			|| code == 77239250 || code == 77239251 || code == 77239252 || code == 77239256
			|| code == 77239270 || code == 77239272 || code == 77239277 ||
			code == 77239278 || code == 77239281 || code == 77239282) { //奥利哈刚
			if (code == 77239245)originalLink = true;
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				if (code == 77239227)return card.is_setcode(0xa50)
					&& (card.type & TYPE_MONSTER) && card.level <= 5;
				return card.is_setcode(0xa50)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				if (code == 77239227)return card.is_setcode(0xa50)
					&& (card.type & TYPE_MONSTER) && card.level <= 5;
				return card.is_setcode(0xa50) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77238312 || code == 77238316 || code == 77238319 || code == 77239302
			|| code == 77239316 || code == 77239318 || code == 77239319 || code == 77239326
			|| code == 77239329 || code == 77239330 || code == 77239340 ||
			code == 77239341 || code == 77239350 || code == 77239353 || code == 77239361
			|| code == 77239360 || code == 77239362 || code == 77239373 ||
			code == 77239378 || code == 77239380 || code == 77240148 || code ==
			77240150 || code == 77240155 || code == 77240157 || code == 77240158
			|| code == 77240159 || code == 77240167 || code == 77240172
			|| code == 77240173 || code == 77240174 || code == 77240176 ||
			code == 77240212 || code == 77240213 || code == 77240224 ||
			code == 77240227 || code == 77240230 || code == 77240231) { //殉道者
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				if (code == 77239319) return card.is_setcode(0xa60)
					&& (card.type & TYPE_MONSTER) && card.level <= 6;
				return card.is_setcode(0xa60)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				if (code == 77239319) return card.is_setcode(0xa60)
					&& (card.type & TYPE_MONSTER) && card.level <= 6;
				return card.is_setcode(0xa60) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77239047 || code == 77239048 || code == 77239051 ||
			code == 77240064) { //恶魔族
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.defense <= 2000
					&& (card.race & RACE_FIEND);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.defense <= 2000 && (card.race & RACE_FIEND);
				});
		}
		else if (code == 77238278 || code == 77238283 || code == 77238300 ||
			code == 77238302) { //异兽
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0xa170)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0xa170) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77239097) { //2只神石板
			int count = CheckCardsCount(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0xa20)
					&& (card.type & TYPE_MONSTER);
				}, code);
			if (count >= 2) return false;
			maxfilter = false;
			randomSize = 2 - count;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0xa20) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77239096 || code == 77239098
			|| code == 77239099) {//神石板
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0xa20)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0xa20) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77238265 || code == 77238275) { //天龙
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0xdb97)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0xdb97) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77238157 || code == 77238782 || code == 77238794) { //真红眼
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0x3b)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0x3b) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77238243 || code == 77238256 || code == 77238257 ||
			code == 77238258 || code == 77238259 || code == 77238260) { //神官怪兽
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				if (code == 77238256) return card.is_setcode(0xa200);
				return card.is_setcode(0xa200)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				if (code == 77238256) return card.is_setcode(0xa200);
				return card.is_setcode(0xa200) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77238200 || code == 77238255) { //千年魔法
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0x201)
					&& (card.type & TYPE_SPELL);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0x201) && (card.type & TYPE_SPELL);
				});
		}
		else if (code == 77239122 || code == 77239126 || code == 77239567) { //青眼
			if (code == 77239567) randomSize = 2;
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0xdd)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0xdd) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77238225 || code == 77238226) { //红龙魔法
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0xa34)
					&& (card.type & TYPE_SPELL);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0xa34) && (card.type & TYPE_SPELL);
				});
		}
		//强化新宇
		else if (code == 77238218) {
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcodes({ 0x1f, 0xa35 })
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcodes({ 0x1f, 0xa35 }) && (card.type & TYPE_MONSTER);
				});
		}
		//强化元素
		else if (code == 77238180 || code == 77238181 || code == 77238186 ||
			code == 77238187 || code == 77238188 || code == 77238197 || code ==
			77238198 || code == 77238213 || code == 77238214 || code == 77238215 ||
			code == 77240144) {
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0x3008) && card.is_setcode(0xa35)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0x3008) && card.is_setcode(0xa35) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77239973) { //2星调整怪兽
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.code != 77239634 && card.level == 2
					&& (card.type & TYPE_TUNER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.code != 77239634 && card.level == 2
					&& (card.type & TYPE_TUNER);
				});
		}
		else if (code == 77240014 || code == 77240022) {//DB
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0xdb99)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0xdb99) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77239988 || code == 77240006) { //黑魔导
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0x10a2)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0x10a2) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77240355 || code == 77240366 ||
			code == 77240367 || code == 77240369 || code == 77240381) { //不死
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				if (code == 77240366 || code == 77240367 || code == 77240369)return card.is_setcodes({ 0x3008 ,0xa250 })
					&& (card.type & TYPE_MONSTER);
				return card.is_setcode(0xa250)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				if (code == 77240366 || code == 77240369)return card.is_setcodes({ 0x3008 ,0xa250 })
					&& (card.type & TYPE_MONSTER);
				return card.is_setcode(0xa250) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77239951 || code == 77239960 ||
			code == 77239961 || code == 77239962 || code == 77239980) { //黑魔导女孩
			if (code == 77239960) originalLink = true;
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0x30a2)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0x30a2) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77238164 || code == 77238166 || code == 77238167 ||
			code == 77238168 || code == 77238173 || code == 77238174) { //盔甲
			if (!CheckCardsZero(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcode(0xae0)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.is_setcode(0xae0) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77239112 || code == 77239119 || code == 77240101) { //神属性
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				return (card.attribute & ATTRIBUTE_DEVINE);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				return card.attribute & ATTRIBUTE_DEVINE;
				});
		}
		else if (code == 77239580) {
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				return (card.level >= 6);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				return (card.level >= 6);
				});
		}
		else if (code == 77239600 || code == 77239614 || code == 77239620 ||
			code == 77239622 || code == 77239634 || code == 77239635 ||
			code == 77239636 || code == 77239637 || code == 77239643 ||
			code == 77239644) { //植物愤怒
			if (code == 77239634 || code == 77239635) originalLink = true;
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				if (code == 77239600) return card.is_setcode(0xa90)
					&& (card.type & TYPE_MONSTER) && card.level <= 6;
				if (code == 77239634) return card.is_setcode(0xa90)
					&& (card.type & TYPE_MONSTER) && card.level <= 4;
				if (code == 77239622) return card.is_setcode(0xa90)
					&& (card.type & TYPE_MONSTER) && card.attack <= 1800;
				return card.is_setcode(0xa90)
					&& (card.type & TYPE_MONSTER);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				if (code == 77239600) return card.is_setcode(0xa90)
					&& (card.type & TYPE_MONSTER) && card.level <= 6;
				if (code == 77239634) return card.is_setcode(0xa90)
					&& (card.type & TYPE_MONSTER) && card.level <= 4;
				if (code == 77239622) return card.is_setcode(0xa90)
					&& (card.type & TYPE_MONSTER) && card.attack <= 1800;
				return card.is_setcode(0xa90) && (card.type & TYPE_MONSTER);
				});
		}
		else if (code == 77239581) {
			randomSize = 3;
			maxfilter = false;
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				return (card.attribute & ATTRIBUTE_DEVINE) && card.is_setcode(0xa12);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				return(card.attribute & ATTRIBUTE_DEVINE) && card.is_setcode(0xa12);
				});
		}
		else if (code == 77239578) { //暗属性
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				return (card.attribute & ATTRIBUTE_DARK);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				return card.attribute & ATTRIBUTE_DARK;
				});
		}
		else if (code == 77239055 || code == 77239056 || code == 77239057) { //冥属性
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				if (code == 77239057) return card.level <= 7 && (card.attribute & ATTRIBUTE_HADES);
				return (card.attribute & ATTRIBUTE_HADES);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				if (code == 77239057) return card.level <= 7 && (card.attribute & ATTRIBUTE_HADES);
				return card.attribute & ATTRIBUTE_HADES;
				});
		}
		else if (code == 77240639) { //奥利哈刚 狼兵
			randomSize = 2;
			maxfilter = false;
			if (CheckCardsCount(deck_list, [this](const CardDataC& card)->bool {
				return card.is_setcodes({ 77240637,77240638,77240639 }); }, code) >= 3) return false;
			codes = { 77240637,77240638,77240639 };
		}
		else if (code == 77239086) { //2只以上魔法师族怪兽
			randomSize = 2;
			if (CheckCardsCount(deck_list, [this](const CardDataC& card)->bool {
				return (card.race & RACE_SPELLCASTER) && card.level >= 6;
				}, code) >= 2) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.race & RACE_SPELLCASTER && card.level >= 6;
				});
		}
		else if (code == 77239677) { //毁灭大法师，三只魔法师
			randomSize = 3;
			if (CheckCardsCount(deck_list, [this](const CardDataC& card)->bool {
				return (card.race & RACE_SPELLCASTER);
				}, code) <= 3) return false;
			codes = FilterCardsCodes([this](const CardDataC& card)->bool {
				return card.race & RACE_SPELLCASTER;
				});
		}
		//魔法师族怪兽
		else if (code == 77238237 || code == 77239053 || code == 77239687
			|| code == 77239952 || code == 77239957 || code == 77239955 ||
			code == 77239956 || code == 77239958 || code == 77239959 ||
			code == 77239965 || code == 77239975 || code == 77239976 ||
			code == 77239982 || code == 77240001) {
			if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
				if (code == 77239952) return (card.race & RACE_SPELLCASTER) &&
					card.level >= 5;
				return (card.race & RACE_SPELLCASTER);
				}, code)) return false;
			codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
				if (code == 77239952) return (card.race & RACE_SPELLCASTER) &&
					card.level >= 5;
				return card.race & RACE_SPELLCASTER;
				});
			}
		else if (code == 77240664 || code == 77240663) { //BT黑羽 暗羽
				if (CheckCardsCount(deck_list, [code, this](const CardDataC& card)->bool {
					return card.is_setcodes({ 0x33 })
						&& (card.type & TYPE_MONSTER);
					}, code) > 2) return false;
				maxfilter = false;
				randomSize = 2;
				codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
					return card.is_setcodes({ 0x33 })
						&& (card.type & TYPE_MONSTER); });
			}
			else if (code == 77240648 || code == 77240651 || code == 77240652) {//黑羽
				maxfilter = false;
				if (CheckCardsCount(deck_list, [code, this](const CardDataC& card)->bool {
					return card.is_setcodes({ 0x33 })
						&& (card.type & TYPE_MONSTER) && card.level <= 5;
					}, code) > 1) return false;
				codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
					return card.is_setcodes({ 0x33 })
						&& (card.type & TYPE_MONSTER) && card.level <= 5; });
			}
				else if (code == 77240057 || code == 77240701) { //不死族
					if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
						if (code == 77240701) return (card.race & RACE_ZOMBIE) && card.attack <= 2500;
						return (card.race & RACE_ZOMBIE);
						}, code)) return false;
					codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
						if (code == 77240701) return (card.race & RACE_ZOMBIE) && card.attack <= 2500;
						return card.race & RACE_ZOMBIE; });
				}
				//龙族怪兽
				else if (code == 77238082 || code == 77238089 || code == 77239125 ||
					code == 77239135 || code == 77239137 || code == 77239139
					|| code == 77239141 || code == 77239145 || code == 77239574 ||
					code == 77239681 || code == 77239908 || code == 77240118 || code == 77240128
					|| code == 77239149 || code == 77238269 || code == 77240061) {
					if (!CheckCardsZero(deck_list, [code, this](const CardDataC& card)->bool {
						if (code == 77239681)return (card.race & RACE_DRAGON)
							&& (card.type & TYPE_MONSTER) && card.level <= 8;
						return (card.race & RACE_DRAGON)
							&& (card.type & TYPE_MONSTER);
						}, code)) return false;
					if (code == 77238269) randomSize = 3;
					codes = FilterCardsCodes([code, this](const CardDataC& card)->bool {
						if (code == 77239681)return (card.race & RACE_DRAGON)
							&& (card.type & TYPE_MONSTER) && card.level <= 8;
						return card.race & RACE_DRAGON;
						});
				}
				else if (code == 77238153 || code == 77238156) { //手枪
					codes = { 77238099,77238101,77238105,77238112,
					77238114,77238117 };
					if (HasCode(deck_list, codes, code)) return false;
				}
				else if (code == 77238107 || code == 77238137) { //冲锋枪
					codes = { 77238107,77238720,77238118,77238119 };
					if (HasCode(deck_list, codes, code)) return false;
				}
				else if (code == 77238107 || code == 77238161) { //散弹枪
					codes = { 77238108,77238708,77238110 };
					if (HasCode(deck_list, codes, code)) return false;
				}
				else if (code == 77238145 || code == 77238151) { //狙击枪
					codes = { 77238146,77238705 };
					if (HasCode(deck_list, codes, code)) return false;
				}
				else if (code == 77238114) { //沙漠之鹰
					randomSize = 3;
					codes = { 77238099,77238114 };
					if (HasCode(deck_list, codes, code)) return false;
				}
				else if (code == 77238150) { //黄金
					codes = { 77238114,77238120,77238147 };
					if (HasCode(deck_list, codes, code)) return false;
				}
				//额外添加场地魔法卡1张
				else if (code == 77238009 || code == 77238010 || code == 77238011 ||
					code == 77238012 || code == 77238013 || code == 77238022 || code == 77238069) {
					//只有当卡组中没有场地魔法时才添加
					if (HasCode(deck_list, zcgManager.field_codes, code)) return false;
					randomSize = 1;
					codes = zcgManager.field_codes;
				}
				//额外添加碎魂者卡
				else if (code == 77238046 || code == 77238049 || code == 77238050 || code == 77238057
					|| code == 77238061) {
					if (!CheckCardsZero(deck_list, [this, code](const CardDataC& card)->bool {
						if (code == 77238050 || code == 77238057 || code == 77238061) return card.is_setcodes(FindSetCodesByValue(ZCGSetCodeType::SoulBreaker)) &&
							card.type & TYPE_MONSTER;
						return card.is_setcodes(FindSetCodesByValue(ZCGSetCodeType::SoulBreaker));
						}, code)) return false;
					auto& group = stype_codes[ZCGSetCodeType::SoulBreaker];
					codes = MergeVec(group.main4UpMonsterCodes, group.main4BelowMonsterCodes);
					if (code == 77238046 || code == 77238049) {//包含魔陷
						codes = MergeVec(codes, group.mainSpellCodes);
					}
				}
				//额外添加CF
				else if (code == 77238099 || code == 77238101 || code == 77238120 || code == 77238122
					|| code == 77238123 || code == 77238124 || code == 77238134 || code == 77238136
					|| code == 77238138 || code == 77238140 || code == 77238147 || code == 77238148
					|| code == 77238152) {
					if (!CheckCardsZero(deck_list, [this, code](const CardDataC& card)->bool {
						if (code == 77238123 || code == 77238124
							|| code == 77238134 || code == 77238136 || code == 77238138
							|| code == 77238147 || code == 77238148 || code == 77238152) return card.is_setcodes(FindSetCodesByValue(ZCGSetCodeType::SoulBreaker)) &&
							card.type & TYPE_MONSTER;
						card.is_setcodes(FindSetCodesByValue(ZCGSetCodeType::CF));
						}, code)) return false;
					auto& group = stype_codes[ZCGSetCodeType::CF];
					codes = MergeVec(group.main4UpMonsterCodes, group.main4BelowMonsterCodes);
					if (code != 77238123 && code != 77238124 && code != 77238134 && code
						!= 77238136 && code != 77238138 && code != 77238147
						&& code != 77238148 && code != 77238152) {
						codes = MergeVec(codes, group.mainSpellCodes);
					}
				}
				//额外添加不死之六武众
				else if (code == 77238032 || code == 77240375 || code == 77240376
					|| code == 77240377 || code == 77240378 || code == 77240673 || code == 77240674) {
					std::vector<unsigned int> setCodes = FindSetCodesByValue(ZCGSetCodeType::Immortal);
					for (int i = 0; i < deck_list.size(); ++i) { //卡组有地缚神系列就不再额外添加
						unsigned int scode = deck_list[i];
						if (_datas.find(scode) != _datas.end()) {
							CardDataC& cd = _datas[scode];
							if (cd.is_setcodes(setCodes) && cd.is_setcode(0x3d)) return false;
						}
						else return false;
					}
					//添加不死之六武众系列卡
					auto& group = stype_codes[ZCGSetCodeType::Immortal];
					auto& imcodes = MergeVec(group.main4UpMonsterCodes, group.main4BelowMonsterCodes);
					auto& cards = CodesToCardsC(imcodes);
					for (auto card : cards) {
						if (card->is_setcodes(setCodes) && card->is_setcode(0x3d)) {
							codes.push_back(card->code);
						}
					}
				}
				if (codes.size() <= 0) return false;
				bool addResult = AddCode(deck_list, codes, randomSize, maxfilter);
				return originalLink ? false : addResult;
			}
			bool ZCGManager::HasCode(const std::vector<int>&A, const std::vector<unsigned int>&B, unsigned int code) {
				std::unordered_set<int> elements(A.begin(), A.end());  // 将 A 中的元素存入集合
				bool first = false;
				for (const int& value : B) {
					if (elements.find(value) != elements.end()) {  // 如果 B 中的元素在集合中找到
						if (code == value && !first) {
							first = true;
							continue;
						}
						return true;
					}
				}
				return false;  // 没有找到相同的元素
			}
			//存在的时候返回false
			bool ZCGManager::CheckCardsZero(std::vector<int>&deck_list, const std::function<bool(const CardDataC&)>&filterFunc,
				unsigned int code) {
				return CheckCardsCount(deck_list, filterFunc, code) <= 0;
			}
			int ZCGManager::CheckCardsCount(std::vector<int>&deck_list, const std::function<bool(const CardDataC&)>&filterFunc, unsigned int code) {
				auto& _datas = dataManager._datas;
				bool first = false;
				int count = 0;
				for (int i = 0; i < deck_list.size(); ++i) { //卡组有系列就不再额外添加
					unsigned int scode = deck_list[i];
					if (!first && code == scode) {//过滤掉这张卡片本身
						first = true;
						continue;
					}
					if (_datas.find(scode) != _datas.end()) {
						CardDataC& cd = _datas[scode];
						if (filterFunc(cd)) ++count;
					}
				}
				return count;
			}
			std::vector<CardDataC*> ZCGManager::FilterCards(const std::function<bool(const CardDataC&)>&filterFunc) {
				auto& _datas = _zdatas;
				std::vector<CardDataC*> results;
				for (int i = 0; i < _zdatas.size(); ++i) { //卡组有系列就不再额外添加
					CardDataC* cd = _datas[i];
					if (filterFunc(*cd))results.push_back(cd);
				}
				return results;
			}
			std::vector<unsigned int> ZCGManager::FilterCardsCodes(const std::function<bool(const CardDataC&)>&filterFunc) {
				auto& _datas = _zdatas;//这个过滤器里面没有限制卡也没有禁止卡
				std::vector<unsigned int> results;
				for (int i = 0; i < _zdatas.size(); ++i) { //卡组有系列就不再额外添加
					CardDataC* cd = _datas[i];
					if (filterFunc(*cd))results.push_back(cd->code);
				}
				return results;
			}
			///zdiy///
		}

