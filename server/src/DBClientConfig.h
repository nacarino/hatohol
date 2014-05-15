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

#ifndef DBClientConfig_h
#define DBClientConfig_h

#include <list>
#include "DBClient.h"
#include "DBClientHatohol.h"

enum MonitoringSystemType {
	MONITORING_SYSTEM_UNKNOWN = -2,
	MONITORING_SYSTEM_FAKE    = -1, // mainly for test
	MONITORING_SYSTEM_ZABBIX,
	MONITORING_SYSTEM_NAGIOS,
	MONITORING_SYSTEM_HAPI_ZABBIX,
	MONITORING_SYSTEM_HAPI_NAGIOS,
	NUM_MONITORING_SYSTEMS,
};

enum IssueTrackerType {
	ISSUE_TRACKER_UNKNOWN = -2,
	ISSUE_TRACKER_FAKE    = -1,
	ISSUE_TRACKER_REDMINE,
	NUM_ISSUE_TRACKERS,
};

struct MonitoringServerInfo {
	ServerIdType         id;
	MonitoringSystemType type;
	std::string          hostName;
	std::string          ipAddress;
	std::string          nickname;
	int                  port;
	int                  pollingIntervalSec;
	int                  retryIntervalSec;

	// The following variables are used in different purposes
	// depending on the MonitoringSystemType.
	//
	// [MONITORING_SYSTEM_ZABBIX]
	//   userName, passowrd: loggin for the ZABBIX server.
	//   dbName            : Not used
	//
	// [MONITORING_SYSTEM_NAGIOS]
	//   userName, passowrd: MySQL user name and the password.
	//   dbName            : MySQL database name.
	std::string          userName;
	std::string          password;
	std::string          dbName; // for naigos ndutils

	// methods

	/**
	 * return an appropriate host information for connection.
	 *
	 * @param forURI
	 * Set true if you want to add square brackets ("[" and "]")
	 * automatically for IPv6 address. It's required to build an URI.
	 * e.g.) ::1 -> [::1]
         *       (for building an URI like http://[::1]:80/path)
	 * 
	 * @return
	 * If ipAddress is set, it is returned. Otherwise, if hostName is set,
	 * it is returned.
	 */
	std::string getHostAddress(bool forURI = false) const;

	static void initialize(MonitoringServerInfo &monitroingServerInfo);
};

typedef std::list<MonitoringServerInfo>    MonitoringServerInfoList;
typedef MonitoringServerInfoList::iterator MonitoringServerInfoListIterator;
typedef MonitoringServerInfoList::const_iterator MonitoringServerInfoListConstIterator;

struct IssueTrackerInfo {
	IssueTrackerIdType id;
	IssueTrackerType   type;
	std::string nickname;
	std::string baseURL;
	std::string projectId;
	std::string trackerId;
	std::string userName;
	std::string password;
};

typedef std::vector<IssueTrackerInfo>        IssueTrackerInfoVect;
typedef IssueTrackerInfoVect::iterator       IssueTrackerInfoVectIterator;
typedef IssueTrackerInfoVect::const_iterator IssueTrackerInfoVectConstIterator;

struct ArmPluginInfo {
	MonitoringSystemType type;
	std::string name; // must be unique
	std::string path;

	/**
	 * The broker URL such as "localhost:5672".
	 * If this value is empty, the default URL is used.
	 */
	std::string brokerUrl;

	/**
	 * A session key is dynamically generated by default. In that case,
	 * this variable is empty.
	 * If a static session key is specified, the form should be 36 bytes
	 * charactes as b4e28ba-2fa1-11d2-883f-b9a761bde3fb.
	 */
	std::string staticSessionKey;
};

typedef std::vector<ArmPluginInfo>        ArmPluginInfoVect;

typedef std::map<MonitoringSystemType, ArmPluginInfo *> ArmPluginInfoMap;
typedef ArmPluginInfoMap::iterator          ArmPluginInfoMapIterator;
typedef ArmPluginInfoMap::const_iterator    ArmPluginInfoMapConstIterator;

class ServerQueryOption : public DataQueryOption {
public:
	ServerQueryOption(const UserIdType &userId = INVALID_USER_ID);
	ServerQueryOption(DataQueryContext *dataQueryContext);
	virtual ~ServerQueryOption();

	void setTargetServerId(const ServerIdType &serverId);

	// Overriding virtual methods
	virtual std::string getCondition(void) const;

protected:
	bool hasPrivilegeCondition(std::string &condition) const;

private:
	struct PrivateContext;
	PrivateContext *m_ctx;
};

class IssueTrackerQueryOption : public DataQueryOption {
public:
	IssueTrackerQueryOption(const UserIdType &userId = INVALID_USER_ID);
	IssueTrackerQueryOption(DataQueryContext *dataQueryContext);
	virtual ~IssueTrackerQueryOption();

	virtual std::string getCondition(void) const; //overrride

protected:
	bool hasPrivilegeCondition(std::string &condition) const;

private:
	struct PrivateContext;
	PrivateContext *m_ctx;
};

class DBClientConfig : public DBClient {
public:
	static int CONFIG_DB_VERSION;
	static const char *DEFAULT_DB_NAME;
	static const char *DEFAULT_USER_NAME;
	static const char *DEFAULT_PASSWORD;
	static void init(const CommandLineArg &cmdArg);
	static void reset(void);

	DBClientConfig(void);
	virtual ~DBClientConfig();

	std::string getDatabaseDir(void);
	void setDatabaseDir(const std::string &dir);
	bool isFaceMySQLEnabled(void);
	int  getFaceRestPort(void);
	void setFaceRestPort(int port);
	bool isCopyOnDemandEnabled(void);

	HatoholError addTargetServer(
	  MonitoringServerInfo *monitoringServerInfo,
	  const OperationPrivilege &privilege);

	HatoholError updateTargetServer(
	  MonitoringServerInfo *monitoringServerInfo,
	  const OperationPrivilege &privilege);

	HatoholError deleteTargetServer(const ServerIdType &serverId,
	                                const OperationPrivilege &privilege);
	void getTargetServers(MonitoringServerInfoList &monitoringServers,
	                      ServerQueryOption &option);

	/**
	 * Get the ID set of accessible servers.
	 *
	 * @param serverIdSet
	 * The obtained IDs are inserted to this object.
	 * @param dataQueryContext A DataQueryContext instance.
	 */
	void getServerIdSet(ServerIdSet &serverIdSet,
	                    DataQueryContext *dataQueryContext);

	/**
	 * Get all entries in the arm_plugins table.
	 *
	 * @param armPluginVect The obtained data is added to this variable.
	 */
	void getArmPluginInfo(ArmPluginInfoVect &armPluginVect);

	/**
	 * Get ArmPluginInfo with the specified type.
	 *
	 * @param armPluginInfo The obtained data is filled to this variable.
	 * @param type A monitoring system type to be obtained.
	 *
	 * @return
	 * true if the ArmPluginInfo is got successfully. Otherwise false.
	 */
	bool getArmPluginInfo(ArmPluginInfo &armPluginInfo,
	                      const MonitoringSystemType &type);

	/**
	 * Save Arm plugin information.
	 * If the entry with armPluginInfo.type exists, the record is updated.
	 * Otherwise the new record is created.
	 *
	 * @param armPluginInfo A data to be saved.
	 *
	 * @rerurn A HatoholError insntace.
	 */
	HatoholError saveArmPluginInfo(const ArmPluginInfo &armPluginInfo);


	/**
	 * Add issue tracker information.
	 *
	 * @param issueTrackerInfo A data to be saved.
	 *
	 * @rerurn A HatoholError insntace.
	 */
	HatoholError addIssueTracker(IssueTrackerInfo *issueTrackerInfo,
				     const OperationPrivilege &privilege);

	/**
	 * Get entries in the issue_trackers table.
	 *
	 * @param issueTrackerVect
	 * The obtained data is added to this variable.
	 * @param option
	 * Options for the query
	 */
	void getIssueTrackers(IssueTrackerInfoVect &issueTrackerVect,
	                      IssueTrackerQueryOption &option);

protected:
	static bool parseCommandLineArgument(const CommandLineArg &cmdArg);
	static void tableInitializerSystem(DBAgent *dbAgent, void *data);
	static bool parseDBServer(const std::string &dbServer,
	                          std::string &host, size_t &port);

	static bool canUpdateTargetServer(
	  MonitoringServerInfo *monitoringServerInfo,
	  const OperationPrivilege &privilege);

	static bool canDeleteTargetServer(
	  const ServerIdType &serverId, const OperationPrivilege &privilege);

	void selectArmPluginInfo(DBAgent::SelectExArg &arg);

	void readArmPluginStream(ItemGroupStream &itemGroupStream,
	                         ArmPluginInfo &armPluginInfo);

private:
	struct PrivateContext;
	PrivateContext *m_ctx;

};

#endif // DBClientConfig_h
