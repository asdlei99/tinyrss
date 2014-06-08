#pragma once

class CRssSource;

class CSource
{
public:
	int id;
	std::string title;
	std::string source;
	std::string category;
	CRssSource* pRss;

	CSource()
		: id(0)
	{}
	CSource(const char* t, const char* s, const char* c, CRssSource* r = nullptr)
	{
		title = t;
		source = s;
		category = c;
		pRss = r;
	}
};

class CCache
{
public:
	int id;
	std::string source;
	std::string title;
	std::string link;
	std::string description;
	bool read;
};

class CRssSource;
class CRssItem;
struct sqlite3;

class CSQLite
{
private:
	class CSQLiteCallback
	{
	public:
		enum class TYPE{
			kGetSources,
			kGetCaches,
			kQueryCacheExistence,
			kQuerySourceExistence,
		};
		TYPE type;

	public:
		CSQLiteCallback(TYPE _type)
			: type(_type)
		{}
	};

	class CQueryCacheExistence : public CSQLiteCallback
	{
	public:
		bool bExists;
	public:
		CQueryCacheExistence()
			: CSQLiteCallback(CSQLiteCallback::TYPE::kQueryCacheExistence)
			, bExists(false)
		{}
	};

	class CQuerySourceExistence : public CSQLiteCallback
	{
	public:
		bool bExists;
	public:
		CQuerySourceExistence()
			: CSQLiteCallback(CSQLiteCallback::TYPE::kQuerySourceExistence)
			, bExists(false)
		{}
	};

public:
	CSQLite();
	~CSQLite();
	CSQLite* operator->(){return this;}
	bool Open(const char* db);
	bool Close();

	bool AddSource(const char* tt, const char* link, const char* category);
	bool IsSourceExists(const char* link);

	// ɾ��ָ��id��RSԴ, ��ָ���Ƿ���Ҫɾ��Դ��Ӧ�Ļ���
	// �������Ҫɾ������, ��ָ��source=nullptr �� source=""
	bool DeleteSource(int id, const char* source = "");

	bool UpdateSource(int id, CSource* pSource);

	// ȡ�����е�RssԴ
	bool GetSources(std::vector<CSource*>* sources);

	// ȡ�����еĻ�����,
	//	read: 0 - δ��
	//		1 - �Ѷ�
	//		-1 - δ��+�Ѷ�
	bool GetCaches(std::vector<CCache*>* caches, const char* source, int read);

	// ��ӵ�����
	bool AddCache(const char* source, const CRssItem* pItem);

	// �Ƴ���ʱ�Ļ���, ��ʱ�Ļ�����ָ: Rss������û��, ���ұ����Ϊ read
	bool RemoveOutdatedCaches(const CRssSource& rss);

	// �ӻ�����ȡ�����Ϊread������, �����Ǵ�Rss�����б�ǳ���
	bool MarkReadFromCache(CRssSource* rss);

	// ��Cache��ĳ����Ϊ δ�� ( �����Ѿ���ͨ��AddCache �� δ�� ��ӽ���)
	bool MarkCacheAsRead(const char* rss, const char* link);

	// ��Cache��δ��������ӵ�rss������
	bool AppendUnreadCache(CRssSource* rss);


private:
	static int __cdecl cbQeuryCallback(void* ud,int argc,char** argv,char** col);
	bool CreateTableSources();
	bool CreateTableCaches();
	bool RemoveCache(int id);
	bool QueryCacheExistence(const char* source, const char* link, bool* bExists);
	bool QuerySourceExistence(const char* source, bool* bExists);
	bool WrapApostrophe(const char* str, std::string* out);

private:
	sqlite3* m_db;
	const char* const kSourcesTableName;
	const char* const kCachesTableName;
};
