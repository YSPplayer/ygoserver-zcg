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
	int leftKey;//ϵ�п�
	int rightKey;//��ϵ�п�
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
	//ֲ���ŭ ǿ�� �����ں�
public:
	ZCGManager();
	~ZCGManager() = default;
	std::vector<unsigned int> ban_codes;//��ֹʹ�õĿ������ݿ����ǰ�͹���
	std::vector<unsigned int> universal_codes;//���ÿ����ϣ����ܻ��ϵ�п�����
	ZCGCodeGroup universal_group;//���ÿ��ֻ�����
	ZCGCodeGroup universal_setcode_group; //����ϵ�п��ֻ�����
	std::vector<unsigned int> universal_codes_setcode;//ϵ�з��ÿ����洢��Ϊ��ϵ�й��˵��ķ��ÿ��������������ʣ�࿨һ����
	std::vector<unsigned int> universal_codes_limit;//���ÿ���������
	std::vector<unsigned int> filter_limit_zero_codes;//�����в�������ڵĿ�
	std::vector<unsigned int> filter_limit_one_codes;//���������ֻ�ܴ���1�ŵĿ��������κ����Ŀ�������˵�Ͽ�
	std::vector<unsigned int> filter_limit_two_codes;//���������ֻ�ܴ���2�ŵĿ�
	std::vector<unsigned int> filter_limit_three_codes;//������������3�ŵĿ�
	std::vector<unsigned int> field_codes;//��ų���ħ��������
	ZCGCodeGroup universal_group_limit;//���ÿ����Ʒֻ�����
	std::vector<CardDataC*> _zdatas;
	std::unordered_map<unsigned int, std::vector<unsigned int>> regarding_codes;//��Կ��������뷺��
	std::vector<ZCGCodeGroup> stype_codes;//��Ų�ͬϵ�еĿ�����
	std::vector<std::vector<unsigned int>> link_codes;//��������ϵ�Ŀ�Ƭ����
	ZCGCodeGroup _codes;//�洢ʣ��������Ƭ���ϣ�Ҳ��һ���̶��ϵķ��ÿ�
	std::unordered_map<unsigned int, unsigned int> setcode_map;//ϵ�к�����֮��ӳ���ϵ��Map
	
	std::vector<int> GetRandomZCGDeckList();
	std::vector<unsigned int> FindSetCodesByValue(unsigned int target_value);
	std::vector<CardDataC*> CodesToCardsC(const std::vector<unsigned int>& codes);
	std::vector<CardDataC*> CodesToCardsC(const std::vector<int>& codes);
	void InitDataGroup();
private:
	static int GetRandomInt(int min, int max);
	static std::random_device rd;
	static std::mt19937 gen;
	std::unordered_map<unsigned int, ZCGContrast> contrast_map;//ϵ�п�Ƭ�໥ռ������
	std::vector<ZCGContrast> card_type_contrasts;//���������ħ�ݿ��ֲ���
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
	Hades,//ڤ��
	BTBlackFeather,//BT����
	Up,//ǿ��Ԫ��Ӣ��
	Olekarks,//��������|����
	Martyr,//ѳ����
	SStar,//��â��|��â��֮��
	SuperGirl,//Ů��Ӷ��|Ů��Ӷ�� ��
	AngerPlants,//ֲ���ŭ
	Osiris,//������˹
	Armor,//װ��
	Ancient,//�Ϲ�|�Ϲ�а��
	PrimitiveInsects,//ԭʼ����
	MechanicalInsects,//��е����
	SoulBreaker,//�����
	RareAnimals,//����
	GodOfficial,//���
	SunGod,//̫����
	Obelisk,//ŷ����˹��
	Immortal,//����
	Tianlong,//����|ʥ��|��������
	CF,//CF
	HArmor,//����
	GodEarth,//�ظ���
	Yaoge,//����
	Cartoon,//��ͨ
	Custom,//�Զ����������ʱ������
	MAX

};
extern ZCGManager zcgManager;
///zdiy///
}

#endif // DATAMANAGER_H
