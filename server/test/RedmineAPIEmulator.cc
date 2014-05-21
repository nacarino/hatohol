/*
 * Copyright (C) 2014 Project Hatohol
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

#include <string>
#include <map>
#include "RedmineAPIEmulator.h"
#include "JsonParserAgent.h"
#include "JsonBuilderAgent.h"

using namespace std;

struct RedmineAPIEmulator::PrivateContext {
	PrivateContext(void)
	: m_issueId(0)
	{
	}

	virtual ~PrivateContext()
	{
	}

	static gboolean auth_callback(SoupAuthDomain *domain, SoupMessage *msg,
				      const char *username,
				      const char *password,
				      gpointer user_data);
	
	static void handlerIssuesJson(SoupServer *server, SoupMessage *msg,
				      const char *path, GHashTable *query,
				      SoupClientContext *client,
				      gpointer user_data);
	string buildReply(const string &subject,
			  const string &description,
			  const string &trackerId);
	void replyPostIssue(SoupMessage *msg);

	map<string, string> m_passwords;
	size_t m_issueId;
};

RedmineAPIEmulator::RedmineAPIEmulator(void)
: HttpServerStub("RedmineAPIEmulator")
{
	m_ctx = new PrivateContext();
}

RedmineAPIEmulator::~RedmineAPIEmulator()
{
	delete m_ctx;
}

void RedmineAPIEmulator::reset(void)
{
	m_ctx->m_passwords.clear();
	m_ctx->m_issueId = 0;
}

void RedmineAPIEmulator::addUser(const std::string &userName,
				 const std::string &password)
{
	m_ctx->m_passwords[userName] = password;
}

gboolean RedmineAPIEmulator::PrivateContext::auth_callback
  (SoupAuthDomain *domain, SoupMessage *msg, const char *username,
   const char *password, gpointer user_data)
{
	RedmineAPIEmulator *emulator
	  = reinterpret_cast<RedmineAPIEmulator *>(user_data);
	map<string, string> &passwords = emulator->m_ctx->m_passwords;

	if (passwords.find(username) == passwords.end())
		return FALSE;
	return passwords[username] == string(password);
}

void RedmineAPIEmulator::setSoupHandlers(SoupServer *soupServer)
{
	SoupAuthDomain *domain = soup_auth_domain_basic_new(
	  SOUP_AUTH_DOMAIN_REALM, "RedminEmulatorRealm",
	  SOUP_AUTH_DOMAIN_BASIC_AUTH_CALLBACK, PrivateContext::auth_callback,
	  SOUP_AUTH_DOMAIN_BASIC_AUTH_DATA, this,
	  SOUP_AUTH_DOMAIN_ADD_PATH, "/",
	  NULL);
	soup_server_add_auth_domain(soupServer, domain);
	g_object_unref(domain);

	soup_server_add_handler(soupServer,
				"/projects/hatoholtestproject/issues.json",
				PrivateContext::handlerIssuesJson, this, NULL);
}

typedef enum {
	ERR_NO_SUBJECT,
	ERR_OTHER,
	N_ERRORS
} RedmineErrorType;

void addError(string &errors, RedmineErrorType type,
	      const string &message = "")
{
	if (errors.empty()) {
		errors = "{\"errors\":[";
	} else {
		errors += ",";
	}
	switch (type) {
	case ERR_NO_SUBJECT:
		errors += "\"Subject can't be blank\"";
		break;
	case ERR_OTHER:
		errors += "\"" + message + "\"";
	default:
		break;
	}
}

string RedmineAPIEmulator::PrivateContext::buildReply(
  const string &subject, const string &description, const string &trackerId)
{
	JsonBuilderAgent agent;
	agent.startObject("");
	agent.startObject("Issue");
	agent.add("id", m_issueId++);

	// TODO: implement

	agent.endObject();
	agent.endObject();
	return string();
}

void RedmineAPIEmulator::PrivateContext::replyPostIssue(SoupMessage *msg)
{
	string body(msg->request_body->data,
		    msg->request_body->length);
	string errors;
	JsonParserAgent agent(body);

	if (!agent.startObject("")) {
		soup_message_set_status(
		  msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
		return;
	}

	string subject, description, trackerId;
	if (!agent.startObject("issue")) {
		addError(errors, ERR_NO_SUBJECT);
	} else {
		agent.read("subject", subject);
		agent.read("description", description);
		agent.read("tracker_id", trackerId);

		if (subject.empty())
			addError(errors, ERR_NO_SUBJECT);
	}

	agent.endObject();
	agent.endObject();

	if (errors.empty()) {
		string reply = buildReply(subject, description, trackerId);
		soup_message_body_append(msg->response_body, SOUP_MEMORY_TAKE,
					 reply.c_str(), reply.size());
		soup_message_set_status(msg, SOUP_STATUS_UNPROCESSABLE_ENTITY);
	} else {
		errors += "]}";
		soup_message_body_append(msg->response_body, SOUP_MEMORY_TAKE,
					 errors.c_str(), errors.size());
		soup_message_set_status(msg, SOUP_STATUS_UNPROCESSABLE_ENTITY);
	}
}

void RedmineAPIEmulator::PrivateContext::handlerIssuesJson
  (SoupServer *server, SoupMessage *msg, const char *path, GHashTable *query,
   SoupClientContext *client, gpointer user_data)
{
	RedmineAPIEmulator *emulator
	  = reinterpret_cast<RedmineAPIEmulator *>(user_data);

	string method = msg->method;
	if (method == "GET") {

	} else if (method == "PUT") {

	} else if (method == "POST") {
		emulator->m_ctx->replyPostIssue(msg);
	} else if (method == "DELETE") {

	} else {
		soup_message_set_status(msg, SOUP_STATUS_METHOD_NOT_ALLOWED);
	}
}
