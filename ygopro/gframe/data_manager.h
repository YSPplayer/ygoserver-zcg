#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include "config.h"
#include <unordered_map>
#include <sqlite3.h>
#include "client_card.h"
///zdiy///
#include <random>
#include <unordered_set>
#include <functional>
///zdiy///
namespace ygo {
	constexpr int MAX_STRING_ID = 0x7ff;
	constexpr unsigned int MIN_CARD_ID = (unsigned int)(MAX_STRING_ID + 1) >> 4;

class DataManager {
public:
	DataManager();
	bool ReadDB(sqlite3* pDB);
	bool LoadDB(const wchar_t* wfile);
	bool LoadStrings(const char* file);
#ifndef YGOPRO_SERVER_MODE
	bool LoadStrings(IReadFile* reader);
#endif
	void ReadStringConfLine(const char* linebuf);
	bool Error(sqlite3* pDB, sqlite3_stmt* pStmt = nullptr);

	code_pointer GetCodePointer(unsigned int code) const;
	string_pointer GetStringPointer(unsigned int code) const;
	code_pointer datas_begin();
	code_pointer datas_end();
	string_pointer strings_begin();
	string_pointer strings_end();
	bool GetData(unsigned int code, CardData* pData) const;
	bool GetString(unsigned int code, CardString* pStr) const;
	const wchar_t* GetName(unsigned int code) const;
	const wchar_t* GetText(unsigned int code) const;
	const wchar_t* GetDesc(unsigned int strCode) const;
	const wchar_t* GetSysString(int code) const;
	const wchar_t* GetVictoryString(int code) const;
	const wchar_t* GetCounterName(int code) const;
	const wchar_t* GetSetName(int code) const;
	std::vector<unsigned int> GetSetCodes(std::wstring setname) const;
	const wchar_t* GetNumString(int num, bool bracket = false);
	const wchar_t* FormatLocation(int location, int sequence) const;
	const wchar_t* FormatAttribute(int attribute);
	const wchar_t* FormatRace(int race);
	const wchar_t* FormatType(int type);
	const wchar_t* FormatSetName(const uint16_t setcode[]);
	const wchar_t* FormatLinkMarker(int link_marker);

	std::unordered_map<unsigned int, std::wstring> _counterStrings;
	std::unordered_map<unsigned int, std::wstring> _victoryStrings;
	std::unordered_map<unsigned int, std::wstring> _setnameStrings;
	std::unordered_map<unsigned int, std::wstring> _sysStrings;
	char errmsg[512]{};

	wchar_t numStrings[301][4]{};
	wchar_t numBuffer[6]{};
	wchar_t attBuffer[128]{};
	wchar_t racBuffer[128]{};
	wchar_t tpBuffer[128]{};
	wchar_t scBuffer[128]{};
	wchar_t lmBuffer[32]{};

	static byte scriptBuffer[0x20000];
	static const wchar_t* unknown_string;
	static uint32 CardReader(uint32, card_data*);
	static byte* ScriptReaderEx(const char* script_name, int* slen);
	static byte* ScriptReader(const char* script_name, int* slen);
#if !defined(YGOPRO_SERVER_MODE) || defined(SERVER_ZIP_SUPPORT)
	static IFileSystem* FileSystem;
#endif

private:
	///zdiy///
	friend class ZCGManager;
	///zdiy///
	std::unordered_map<unsigned int, CardDataC> _datas;
	std::unordered_map<unsigned int, CardString> _strings;
	std::unordered_map<unsigned int, std::vector<uint16_t>> extra_setcode;
};

extern DataManager dataManager;
///zdiy///

#define ZCG_MIN_CODE 77238000
#define ZCG_MAX_CODE 77240704
struct ZCGContrast {
	int leftKey;//系列卡
	int rightKey;//非系列卡
};
class ZCGCodeGroup {
public:
	ZCGCodeGroup() = default;
	~ZCGCodeGroup() = default;
	std::vector<unsigned int> main4BelowMonsterCodes;
	std::vector<unsigned int> main4UpMonsterCodes;
	std::vector<unsigned int> mainSpellCodes;
	std::vector<unsigned int> extraMonsterCodes;
};
class ZCGManager {
	//植物愤怒 强化 本家融合
public:
	ZCGManager();
	~ZCGManager() = default;
	std::vector<unsigned int> ban_codes;//禁止使用的卡，数据库加载前就过滤
	std::vector<unsigned int> universal_codes;//泛用卡集合，可能会和系列卡重名
	ZCGCodeGroup universal_group;//泛用卡分化集合
	ZCGCodeGroup universal_setcode_group; //泛用系列卡分化集合
	std::vector<unsigned int> universal_codes_setcode;//系列泛用卡，存储因为被系列过滤掉的泛用卡，但随机基本和剩余卡一样低
	std::vector<unsigned int> universal_codes_limit;//泛用卡集合限制
	std::vector<unsigned int> filter_limit_zero_codes;//卡组中不允许存在的卡
	std::vector<unsigned int> filter_limit_one_codes;//卡组中最多只能存在1张的卡，包括任何类别的卡，比如说废卡
	std::vector<unsigned int> filter_limit_two_codes;//卡组中最多只能存在2张的卡
	std::vector<unsigned int> filter_limit_three_codes;//卡组中最多存在3张的卡
	std::vector<unsigned int> field_codes;//存放场地魔法卡卡号
	ZCGCodeGroup universal_group_limit;//泛用卡限制分化集合
	std::vector<CardDataC*> _zdatas;
	std::unordered_map<unsigned int, std::vector<unsigned int>> regarding_codes;//针对卡，不加入泛用
	std::vector<ZCGCodeGroup> stype_codes;//存放不同系列的卡集合
	std::vector<std::vector<unsigned int>> link_codes;//有联动关系的卡片集合
	ZCGCodeGroup _codes;//存储剩余的随机卡片集合，也是一定程度上的泛用卡
	std::unordered_map<unsigned int, unsigned int> setcode_map;//系列和索引之间映射关系的Map
	
	std::vector<int> GetRandomZCGDeckList();
	std::vector<unsigned int> FindSetCodesByValue(unsigned int target_value);
	std::vector<CardDataC*> CodesToCardsC(const std::vector<unsigned int>& codes);
	std::vector<CardDataC*> CodesToCardsC(const std::vector<int>& codes);
	void InitDataGroup();
private:
	static int GetRandomInt(int min, int max);
	static std::random_device rd;
	static std::mt19937 gen;
	std::unordered_map<unsigned int, ZCGContrast> contrast_map;//系列卡片相互占比索引
	std::vector<ZCGContrast> card_type_contrasts;//主卡组怪兽魔陷卡分布比
	static int GetVecElementCount(const std::vector<int>& vec, int element);
	static int GetVecElementCount(const std::vector<unsigned int>& vec, int element);
	int GetDeckMaxCount(unsigned int code);
	void RemoveCode(std::vector<unsigned int>& codes, unsigned int code);
	static std::vector<unsigned int> MergeVec(const std::vector<unsigned int>& a, const std::vector<unsigned int>& b);
	void AddLinkCode(std::vector<int>& deck_list, unsigned int code);
	bool AddCode(std::vector<int>& deck_list,const std::vector<unsigned int> &codes, int randomSize,bool maxfilter = true);
	bool AddCode(std::vector<int>& deck_list, const std::vector<CardDataC*>& cards, int randomSize);
	bool AddSpecialLinkCode(std::vector<int>& deck_list, unsigned int code);
	bool HasCode(const std::vector<int>& A, const std::vector<unsigned int>& B, unsigned int code);
	bool CheckCardsZero(std::vector<int>& deck_list, const std::function<bool(const CardDataC&)>& filterFunc, unsigned int code);
	int CheckCardsCount(std::vector<int>& deck_list, const std::function<bool(const CardDataC&)>& filterFunc, unsigned int code);
	std::vector<CardDataC*> FilterCards(const std::function<bool(const CardDataC&)>& filterFunc);
	std::vector<unsigned int> FilterCardsCodes(const std::function<bool(const CardDataC&)>& filterFunc);
};
enum ZCGSetCodeType {
	Hades,//冥界
	BTBlackFeather,//BT黑羽
	Up,//强化元素英雄
	Olekarks,//奥利哈刚|达姿
	Martyr,//殉道者
	SStar,//六芒星|六芒星之龙
	SuperGirl,//女子佣兵|女子佣兵 灵
	AngerPlants,//植物愤怒
	Osiris,//奥西里斯
	Armor,//装甲
	Ancient,//上古|上古邪器
	PrimitiveInsects,//原始昆虫
	MechanicalInsects,//机械昆虫
	SoulBreaker,//碎魂者
	RareAnimals,//异兽
	GodOfficial,//神官
	SunGod,//太阳神
	Obelisk,//欧贝利斯克
	Immortal,//不死
	Tianlong,//天龙|圣龙|机甲天龙
	CF,//CF
	HArmor,//盔甲
	GodEarth,//地缚神
	Yaoge,//妖歌
	Cartoon,//卡通
	Custom,//自定义随机，暂时不开启
	MAX

};
extern ZCGManager zcgManager;
///zdiy///
}

#endif // DATAMANAGER_H
