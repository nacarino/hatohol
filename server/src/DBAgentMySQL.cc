/*
 * Copyright (C) 2013-2014 Project Hatohol
 *
 * This file is part of Hatohol.
 *
 * Hatohol is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Hatohol is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Hatohol. If not, see <http://www.gnu.org/licenses/>.
 */

#include <mysql/errmsg.h>
#include <unistd.h>
#include "DBAgentMySQL.h"
#include "SQLUtils.h"
#include "Params.h"
using namespace std;
using namespace mlpl;

static const size_t DEFAULT_NUM_RETRY = 5;
static const size_t RETRY_INTERVAL[DEFAULT_NUM_RETRY] = {
  0, 10, 60, 60, 60 };

struct DBAgentMySQL::PrivateContext {
	static string engineStr;
	static set<unsigned int> retryErrorSet;
	MYSQL mysql;
	bool  connected;
	string dbName;
	string user;
	string password;
	string host;
	unsigned int port;
	bool inTransaction;

	PrivateContext(void)
	: connected(false),
	  port(0),
	  inTransaction(false)
	{
	}
	
	bool shouldRetry(unsigned int errorNumber)
	{
		return retryErrorSet.find(errorNumber) != retryErrorSet.end();
	}
};

string DBAgentMySQL::PrivateContext::engineStr;
set<unsigned int> DBAgentMySQL::PrivateContext::retryErrorSet;

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------
void DBAgentMySQL::init(void)
{
	char *env = getenv("HATOHOL_MYSQL_ENGINE_MEMORY");
	if (env && atoi(env) == 1) {
		MLPL_INFO("Use memory engine\n");
		PrivateContext::engineStr = " ENGINE=MEMORY";
	}

	PrivateContext::retryErrorSet.insert(CR_SERVER_GONE_ERROR);
}

DBAgentMySQL::DBAgentMySQL(const char *db, const char *user, const char *passwd,
                           const char *host, unsigned int port,
                           DBDomainId domainId, bool skipSetup)
: DBAgent(domainId, skipSetup),
  m_ctx(NULL)
{
	m_ctx = new PrivateContext();

	m_ctx->dbName   = db     ? : "";
	m_ctx->user     = user   ? : "";
	m_ctx->password = passwd ? : "";
	m_ctx->host     = host   ? : "";
	m_ctx->port     = port;
	connect();
	if (!m_ctx->connected) {
		THROW_HATOHOL_EXCEPTION("Failed to connect to MySQL: %s: %s\n",
		                      db, mysql_error(&m_ctx->mysql));
	}
}

DBAgentMySQL::~DBAgentMySQL()
{
	if (m_ctx) {
		mysql_close(&m_ctx->mysql);
		delete m_ctx;
	}
}

string DBAgentMySQL::getDBName(void) const
{
	return m_ctx->dbName;
}

void DBAgentMySQL::getIndexes(std::vector<IndexStruct> &indexStructVect,
                              const std::string &tableName)
{
	HATOHOL_ASSERT(m_ctx->connected, "Not connected.");
	string query =
	  StringUtils::sprintf("SHOW INDEX FROM %s", tableName.c_str());
	execSql(query);

	MYSQL_RES *result = mysql_store_result(&m_ctx->mysql);
	if (!result) {
		THROW_HATOHOL_EXCEPTION(
		  "Failed to call mysql_store_result: %s\n",
		  mysql_error(&m_ctx->mysql));
	}

	indexStructVect.reserve(mysql_num_rows(result));
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		int col = 0;
		IndexStruct idxStruct;
		idxStruct.table      = row[col++];
		idxStruct.nonUnique  = atoi(row[col++]);
		idxStruct.keyName    = row[col++];
		idxStruct.seqInIndex = atoi(row[col++]);
		idxStruct.columnName = row[col++];
		indexStructVect.push_back(idxStruct);
	}
	mysql_free_result(result);
}

bool DBAgentMySQL::isTableExisting(const string &tableName)
{
	HATOHOL_ASSERT(m_ctx->connected, "Not connected.");
	string query =
	  StringUtils::sprintf(
	    "SHOW TABLES FROM %s LIKE '%s'",
	    getDBName().c_str(), tableName.c_str());
	execSql(query);

	MYSQL_RES *result = mysql_store_result(&m_ctx->mysql);
	if (!result) {
		THROW_HATOHOL_EXCEPTION(
		  "Failed to call mysql_store_result: %s\n",
		  mysql_error(&m_ctx->mysql));
	}

	MYSQL_ROW row;
	bool found = false;
	while ((row = mysql_fetch_row(result))) {
		found = true;
		break;
	}
	mysql_free_result(result);

	return found;
}

bool DBAgentMySQL::isRecordExisting(const string &tableName,
                                    const string &condition)
{
	HATOHOL_ASSERT(m_ctx->connected, "Not connected.");
	string query =
	  StringUtils::sprintf(
	    "SELECT * FROM %s WHERE %s",
	    tableName.c_str(), condition.c_str());
	execSql(query);

	MYSQL_RES *result = mysql_store_result(&m_ctx->mysql);
	if (!result) {
		THROW_HATOHOL_EXCEPTION(
		  "Failed to call mysql_store_result: %s\n",
		  mysql_error(&m_ctx->mysql));
	}

	MYSQL_ROW row;
	bool found = false;
	while ((row = mysql_fetch_row(result))) {
		found = true;
		break;
	}
	mysql_free_result(result);

	return found;
}

void DBAgentMySQL::begin(void)
{
	execSql("START TRANSACTION");
	m_ctx->inTransaction = true;
}

void DBAgentMySQL::commit(void)
{
	execSql("COMMIT");
	m_ctx->inTransaction = false;
}

void DBAgentMySQL::rollback(void)
{
	execSql("ROLLBACK");
	m_ctx->inTransaction = false;
}

void DBAgentMySQL::execSql(const string &statement)
{
	HATOHOL_ASSERT(m_ctx->connected, "Not connected.");
	queryWithRetry(statement);
}

static string getColumnTypeQuery(const ColumnDef &columnDef)
{
	switch (columnDef.type) {
	case SQL_COLUMN_TYPE_INT:
		return StringUtils::sprintf("INT(%zd)",
					    columnDef.columnLength);
	case SQL_COLUMN_TYPE_BIGUINT:
		return StringUtils::sprintf("BIGINT(%zd) UNSIGNED",
					    columnDef.columnLength);
	case SQL_COLUMN_TYPE_VARCHAR:
		return StringUtils::sprintf("VARCHAR(%zd)",
					    columnDef.columnLength);
	case SQL_COLUMN_TYPE_CHAR:
		return StringUtils::sprintf("CHAR(%zd)",
					    columnDef.columnLength);
	case SQL_COLUMN_TYPE_TEXT:
		return "TEXT";
	case SQL_COLUMN_TYPE_DOUBLE:
		return StringUtils::sprintf("DOUBLE(%zd,%zd)",
					    columnDef.columnLength,
					    columnDef.decFracLength);
	case SQL_COLUMN_TYPE_DATETIME:
		return " DATETIME";
	default:
		HATOHOL_ASSERT(false, "Unknown type: %d", columnDef.type);
	}
	return string();
}

static string getColumnDefinitionQuery(const ColumnDef &columnDef)
{
	string query;

	query += columnDef.columnName;
	query += " ";
	query += getColumnTypeQuery(columnDef);

	// nullable
	if (!columnDef.canBeNull)
		query += " NOT NULL";

	// key type
	switch (columnDef.keyType) {
	case SQL_KEY_PRI:
		query += " PRIMARY KEY";
		break;
	case SQL_KEY_UNI:
	case SQL_KEY_IDX: // To be created in createIndexIfNotExists()
	case SQL_KEY_NONE:
		break;
	default:
		HATOHOL_ASSERT(false,
		  "Unknown key type: %d", columnDef.keyType);
	}

	return query;
}

void DBAgentMySQL::createTable(const TableProfile &tableProfile)
{
	HATOHOL_ASSERT(m_ctx->connected, "Not connected.");
	string query = StringUtils::sprintf("CREATE TABLE %s (",
	                                    tableProfile.name);
	
	for (size_t i = 0; i < tableProfile.numColumns; i++) {
		const ColumnDef &columnDef = tableProfile.columnDefs[i];
		query += getColumnDefinitionQuery(columnDef);

		// auto increment
		if (columnDef.flags & SQL_COLUMN_FLAG_AUTO_INC)
			query += " AUTO_INCREMENT";

		if (i < tableProfile.numColumns -1)
			query += ",";
	}
	query += ")";
	if (!m_ctx->engineStr.empty())
		query += m_ctx->engineStr;

	execSql(query);
}

void DBAgentMySQL::insert(const DBAgent::InsertArg &insertArg)
{
	const size_t numColumns = insertArg.tableProfile.numColumns;
	HATOHOL_ASSERT(m_ctx->connected, "Not connected.");
	HATOHOL_ASSERT(numColumns == insertArg.row->getNumberOfItems(),
	               "numColumn: %zd != row: %zd",
	               numColumns, insertArg.row->getNumberOfItems());

	string query = StringUtils::sprintf("INSERT INTO %s (",
	                                    insertArg.tableProfile.name);
	for (size_t i = 0; i < numColumns; i++) {
		const ColumnDef &columnDef =
		  insertArg.tableProfile.columnDefs[i];
		query += columnDef.columnName;
		if (i < numColumns -1)
			query += ",";
	}
	query += ") VALUES (";
	for (size_t i = 0; i < numColumns; i++) {
		if (i > 0)
			query += ",";

		const ColumnDef &columnDef =
		  insertArg.tableProfile.columnDefs[i];
		const ItemData *itemData = insertArg.row->getItemAt(i);
		if (itemData->isNull()) {
			query += "NULL";
			continue;
		}

		switch (columnDef.type) {
		case SQL_COLUMN_TYPE_INT:
		case SQL_COLUMN_TYPE_BIGUINT:
		case SQL_COLUMN_TYPE_DOUBLE:
			query += itemData->getString();
			break;
		case SQL_COLUMN_TYPE_VARCHAR:
		case SQL_COLUMN_TYPE_CHAR:
		case SQL_COLUMN_TYPE_TEXT:
		{ // bracket is used to avoid an error: jump to case label
			string src =  itemData->getString();
			char *escaped = new char[src.size() * 2 + 1]; 
			mysql_real_escape_string(&m_ctx->mysql, escaped,
			                         src.c_str(), src.size());
			query += "'";
			query += escaped,
			query += "'";
			delete [] escaped;
			break;
		}
		case SQL_COLUMN_TYPE_DATETIME:
		{ // bracket is used to avoid an error: jump to case label
			query += makeDatetimeString(*itemData);
			break;
		}
		default:
			HATOHOL_ASSERT(false, "Unknown type: %d", columnDef.type);
		}
	}
	query += ")";

	execSql(query);
}


void DBAgentMySQL::update(const UpdateArg &updateArg)
{
	HATOHOL_ASSERT(m_ctx->connected, "Not connected.");
	string sql = makeUpdateStatement(updateArg);
	execSql(sql);
}

void DBAgentMySQL::select(const DBAgent::SelectArg &selectArg)
{
	HATOHOL_ASSERT(m_ctx->connected, "Not connected.");

	string query = makeSelectStatement(selectArg);
	execSql(query);

	MYSQL_RES *result = mysql_store_result(&m_ctx->mysql);
	if (!result) {
		THROW_HATOHOL_EXCEPTION("Failed to call mysql_store_result: %s\n",
		                      mysql_error(&m_ctx->mysql));
	}

	MYSQL_ROW row;
	VariableItemTablePtr dataTable;
	size_t numColumns = selectArg.columnIndexes.size();
	while ((row = mysql_fetch_row(result))) {
		VariableItemGroupPtr itemGroup;
		for (size_t i = 0; i < numColumns; i++) {
			size_t idx = selectArg.columnIndexes[i];
			const ColumnDef &columnDef =
			  selectArg.tableProfile.columnDefs[idx];
			ItemDataPtr itemDataPtr =
			  SQLUtils::createFromString(row[i], columnDef.type);
			itemGroup->add(itemDataPtr);
		}
		dataTable->add(itemGroup);
	}
	mysql_free_result(result);
	selectArg.dataTable = dataTable;
}

void DBAgentMySQL::select(const SelectExArg &selectExArg)
{
	HATOHOL_ASSERT(m_ctx->connected, "Not connected.");

	string query = makeSelectStatement(selectExArg);
	execSql(query);

	MYSQL_RES *result = mysql_store_result(&m_ctx->mysql);
	if (!result) {
		THROW_HATOHOL_EXCEPTION("Failed to call mysql_store_result: %s\n",
		                      mysql_error(&m_ctx->mysql));
	}

	MYSQL_ROW row;
	VariableItemTablePtr dataTable;
	size_t numColumns = selectExArg.statements.size();
	while ((row = mysql_fetch_row(result))) {
		VariableItemGroupPtr itemGroup;
		for (size_t i = 0; i < numColumns; i++) {
			SQLColumnType type = selectExArg.columnTypes[i];
			ItemDataPtr itemDataPtr =
			  SQLUtils::createFromString(row[i], type);
			itemGroup->add(itemDataPtr);
		}
		dataTable->add(itemGroup);
	}
	mysql_free_result(result);
	selectExArg.dataTable = dataTable;

	// check the result
	size_t numTableRows = selectExArg.dataTable->getNumberOfRows();
	size_t numTableColumns = selectExArg.dataTable->getNumberOfColumns();
	HATOHOL_ASSERT((numTableRows == 0) ||
	             ((numTableRows > 0) && (numTableColumns == numColumns)),
	             "Sanity check error: numTableRows: %zd, numTableColumns: "
	             "%zd, numColumns: %zd",
	             numTableRows, numTableColumns, numColumns);
}

void DBAgentMySQL::deleteRows(const DeleteArg &deleteArg)
{
	HATOHOL_ASSERT(m_ctx->connected, "Not connected.");
	string query = makeDeleteStatement(deleteArg);
	execSql(query);
}

uint64_t DBAgentMySQL::getLastInsertId(void)
{
	HATOHOL_ASSERT(m_ctx->connected, "Not connected.");
	execSql("SELECT LAST_INSERT_ID()");
	MYSQL_RES *result = mysql_store_result(&m_ctx->mysql);
	if (!result) {
		THROW_HATOHOL_EXCEPTION(
		  "Failed to call mysql_store_result: %s\n",
		  mysql_error(&m_ctx->mysql));
	}
	MYSQL_ROW row = mysql_fetch_row(result);
	HATOHOL_ASSERT(row, "Failed to call mysql_fetch_row.");
	uint64_t id;
	int numScan = sscanf(row[0], "%" PRIu64, &id);
	HATOHOL_ASSERT(numScan == 1, "numScan: %d, %s", numScan, row[0]);
	return id;
}

uint64_t DBAgentMySQL::getNumberOfAffectedRows(void)
{
	HATOHOL_ASSERT(m_ctx->connected, "Not connected.");
	my_ulonglong num = mysql_affected_rows(&m_ctx->mysql);
	// According to the referece manual, mysql_affected_rows()
	// doesn't return an error.
	//   http://dev.mysql.com/doc/refman/5.1/en/mysql-affected-rows.html
	return num;
}

void DBAgentMySQL::addColumns(const AddColumnsArg &addColumnsArg)
{
	string query = "ALTER TABLE ";
	query += addColumnsArg.tableProfile.name;
	vector<size_t>::const_iterator it = addColumnsArg.columnIndexes.begin();
	vector<size_t>::const_iterator lastElemIt =
	  --addColumnsArg.columnIndexes.end();

	for (; it != addColumnsArg.columnIndexes.end(); ++it) {
		const size_t index = *it;
		const ColumnDef &columnDef =
		  addColumnsArg.tableProfile.columnDefs[index];
		query += " ADD COLUMN ";
		query += getColumnDefinitionQuery(columnDef);
		if (it != lastElemIt)
			query += ",";
	}
	execSql(query);
}

// ---------------------------------------------------------------------------
// Protected methods
// ---------------------------------------------------------------------------
const char *DBAgentMySQL::getCStringOrNullIfEmpty(const string &str)
{
	return str.empty() ? NULL : str.c_str();
}

void DBAgentMySQL::connect(void)
{
	const char *unixSocket = NULL;
	unsigned long clientFlag = 0;
	const char *host   = getCStringOrNullIfEmpty(m_ctx->host);
	const char *user   = getCStringOrNullIfEmpty(m_ctx->user);
	const char *passwd = getCStringOrNullIfEmpty(m_ctx->password);
	const char *db     = getCStringOrNullIfEmpty(m_ctx->dbName);
	mysql_init(&m_ctx->mysql);
	MYSQL *result = mysql_real_connect(&m_ctx->mysql, host, user, passwd,
	                                   db, m_ctx->port,
	                                   unixSocket, clientFlag);
	if (!result) {
		MLPL_ERR("Failed to connect to MySQL: %s: (error: %u, "
		         "Domain ID: 0x%x) %s\n",
		         db, mysql_errno(&m_ctx->mysql), getDBDomainId(),
		         mysql_error(&m_ctx->mysql));
	}
	m_ctx->connected = result;
	m_ctx->inTransaction = false;
}

void DBAgentMySQL::sleepAndReconnect(unsigned int sleepTimeSec)
{
	// TODO:
	// add mechanism to wake up immediately if the program is
	// going to exit. We should make an interrputible sleep object
	// which is similar to ArmBase::sleepInterruptible().
	while (sleepTimeSec) {
		// If a signal happens during the sleep(), it returns
		// the remaining time.
		sleepTimeSec = sleep(sleepTimeSec);
	}

	mysql_close(&m_ctx->mysql);
	m_ctx->connected = false;
	connect();
}

void DBAgentMySQL::queryWithRetry(const string &statement)
{
	unsigned int errorNumber = 0;
	size_t numRetry = DEFAULT_NUM_RETRY;
	for (size_t i = 0; i < numRetry; i++) {
		if (mysql_query(&m_ctx->mysql, statement.c_str()) == 0) {
			if (i >= 1) {
				MLPL_INFO("Recoverd: %s (retry #%zd).\n",
				          statement.c_str(), i);
			}
			return;
		}
		errorNumber = mysql_errno(&m_ctx->mysql);
		if (!m_ctx->shouldRetry(errorNumber))
			break;
		if (m_ctx->inTransaction)
			break;
		MLPL_ERR("Failed to query: %s: (%u) %s.\n",
		         statement.c_str(), errorNumber,
		         mysql_error(&m_ctx->mysql));
		if (i == numRetry - 1)
			break;

		// retry repeatedly until the connection is established or
		// the maximum retry count.
		for (; i < numRetry; i++) {
			size_t sleepTimeSec = RETRY_INTERVAL[i];
			MLPL_INFO("Try to connect after %zd sec. (%zd/%zd)\n",
			          sleepTimeSec, i+1, numRetry);
			sleepAndReconnect(sleepTimeSec);
			if (m_ctx->connected)
				break;
		}
	}

	THROW_HATOHOL_EXCEPTION("Failed to query: %s: (%u) %s\n",
	                        statement.c_str(), errorNumber,
	                        mysql_error(&m_ctx->mysql));
}

string DBAgentMySQL::makeCreateIndexStatement(const IndexDef &indexDef)
{
	string sql = StringUtils::sprintf(
	  "CREATE %sINDEX %s ON %s (",
	  indexDef.isUnique ? "UNIQUE " : "",
	  indexDef.name, indexDef.tableProfile->name);

	const int *columnIdxPtr = indexDef.columnIndexes;
	while (true) {
		const ColumnDef &columnDef =
		  indexDef.tableProfile->columnDefs[*columnIdxPtr];
		sql += columnDef.columnName;

		// get the next column index and check if it's the end.
		columnIdxPtr++;
		if (*columnIdxPtr != IndexDef::END) {
			sql += ",";
		} else {
			sql += ")";
			break;
		}
	}
	return sql;
}

std::string DBAgentMySQL::makeDropIndexStatement(
  const std::string &name, const std::string &tableName)
{
	return StringUtils::sprintf("DROP INDEX %s ON %s",
	                            name.c_str(), tableName.c_str());
}

void DBAgentMySQL::getIndexInfoVect(vector<IndexInfo> &indexInfoVect,
                                    const TableProfile &tableProfile)
{
	struct ColumnIndexDict {
		map<string, int> dict;

		ColumnIndexDict(const TableProfile &tableProfile)
		{
			for (size_t i = 0; i < tableProfile.numColumns; i++) {
				const ColumnDef &columnDef = 
				  tableProfile.columnDefs[i];
				dict[columnDef.columnName] = i;
			}
		}

		int find(const string &columnName)
		{
			map<string, int>::iterator it = dict.find(columnName);
			HATOHOL_ASSERT(it != dict.end(),
			               "Not found: %s", columnName.c_str());
			return it->second;
		}
	} columnIdxDict(tableProfile);

	typedef map<size_t, const IndexStruct *>  IndexStructSeqMap;
	typedef IndexStructSeqMap::const_iterator IndexStructSeqMapConstIterator;
	typedef map<string, IndexStructSeqMap>    IndexStructMap;
	typedef IndexStructMap::iterator          IndexStructMapIterator;

	vector<IndexStruct> indexStructVect;
	getIndexes(indexStructVect, tableProfile.name);
	
	// Group the same index
	IndexStructMap indexStructMap;
	for (size_t i = 0; i < indexStructVect.size(); i++) {
		const IndexStruct &idxStruct = indexStructVect[i];
		if (idxStruct.keyName == "PRIMARY")
			continue;
		const size_t seq = idxStruct.seqInIndex;
		indexStructMap[idxStruct.keyName][seq] = &idxStruct;
	}

	// Make an ouput vector
	IndexStructMapIterator it = indexStructMap.begin();
	for (; it != indexStructMap.end(); ++it) {
		const IndexStructSeqMap seqMap = it->second;
		IndexStructSeqMapConstIterator seqIt = seqMap.begin();
		const IndexStruct &firstElem = *seqIt->second;
		const bool isUnique = !firstElem.nonUnique;

		int columnIndexes[seqMap.size() + 1];
		columnIndexes[seqMap.size()] = IndexDef::END;
		for (int cnt = 0; seqIt != seqMap.end(); ++seqIt, cnt++) {
			const string &columnName = seqIt->second->columnName;
			columnIndexes[cnt] = columnIdxDict.find(columnName);
		}

		IndexInfo idxInfo;
		idxInfo.name      = firstElem.keyName;
		idxInfo.tableName = firstElem.table;

		const IndexDef indexDef = {
		  idxInfo.name.c_str(), &tableProfile, columnIndexes, isUnique
		};
		idxInfo.sql = makeCreateIndexStatement(indexDef);
		indexInfoVect.push_back(idxInfo);
	}
}

