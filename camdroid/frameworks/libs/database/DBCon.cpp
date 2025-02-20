#include <unistd.h>
#include "include_database/DBCon.h"
#undef LOG_NDEBUG
#undef NDEBUG
#define LOG_TAG "DBCon.cpp"
#include <utils/Log.h>

namespace android {

DBCon* DBCon::mDBCon = NULL;

bool DBCon::open(const char *dbName)
{
	bool ret = true;
	unsigned int retries = 50;
	unsigned int count = 0;

	mSQLCon = NULL;
	if (SQLITE_OK != sqlite3_open(dbName, &mSQLCon)) {
		ERROR(__FUNCTION__, __LINE__, mSQLCon);
		mSQLCon = NULL;
		ret = false;
	}
	ALOGD("after sqlite3_open: mSQLCon is %p, ret is %d\n", mSQLCon, ret);
	return true;
}

bool DBCon::close(void)
{
	bool ret = true;

	if (SQLITE_OK != sqlite3_close(mSQLCon)) {
		ERROR(__FUNCTION__, __LINE__, mSQLCon);
		ret = false;
	}
	mSQLCon = NULL;

	return ret;
}

SQLCon* DBCon::getConnect(void)
{
	if (!mSQLCon) {
		printf("fail to Connect");
	}
	return mSQLCon;
}

bool DBCon::lock(void)
{
	if (mLock) {
		return true;
	}
	mLock = true;
	return false;
}

void DBCon::unlock(void)
{
	mLock = false;
}

DBCon* DBCon::getInstance(const char *dbName)
{
	ALOGD("getInstance: mDBCon is %p\n", mDBCon);
	if (!mDBCon) {
		mDBCon = new DBCon(dbName);
	}
	return mDBCon;
}

void DBCon::freeInstance(void)
{
	if(mDBCon) {
		ALOGD("delete mDBCon\n");
		delete mDBCon;
		mDBCon = NULL;
	}
}

DBCon::DBCon(const char *dbName)
{
	open(dbName);
	mLock = false;
}

DBCon::~DBCon()
{
	if (mSQLCon) {
		ALOGD("sqlite3_close\n");
		if (SQLITE_OK != sqlite3_close(mSQLCon)) {
			ERROR(__FUNCTION__, __LINE__, mSQLCon);
		}
		mSQLCon = NULL;
	}
}


}
