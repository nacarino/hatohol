/*
 * Copyright (C) 2013 Project Hatohol
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

#ifndef SQLFormulaParser_h
#define SQLFormulaParser_h

#include <string>
#include <map>
#include <ParsableString.h>
#include "FormulaElement.h"
#include "FormulaFunction.h"
#include "SQLProcessorTypes.h"

class SQLFormulaParser
{
public:
	static void init(void);
	SQLFormulaParser(void);
	virtual ~SQLFormulaParser();
	void setColumnDataGetterFactory
	       (FormulaVariableDataGetterFactory columnDataGetterFactory,
	        void *columnDataGetterFactoryPriv);
	virtual void add(const std::string &word, const std::string &wordLower);
	virtual void flush(void);
	virtual void close(void);
	mlpl::SeparatorCheckerWithCallback *getSeparatorChecker(void);
	FormulaElement *getFormula(void) const;
	bool hasStatisticalFunc(void) const;
	void setShareInfo(SQLProcessorSelectShareInfo *shareInfo);
	SQLProcessorSelectShareInfo *getShareInfo(void) const;

protected:
	//
	// type definition
	//
	typedef void (SQLFormulaParser::*KeywordHandler)(void);
	typedef std::map<std::string, KeywordHandler> KeywordHandlerMap;
	typedef KeywordHandlerMap::iterator KeywordHandlerMapIterator;

	typedef void (SQLFormulaParser::*FunctionParser)(void);
	typedef std::map<std::string, FunctionParser> FunctionParserMap;
	typedef FunctionParserMap::iterator FunctionParserMapIterator;


	//
	// general sub routines
	//
	static void copyKeywordHandlerMap(KeywordHandlerMap &kwHandlerMap);
	static void copyFunctionParserMap(FunctionParserMap &fncParserMap);

	void setKeywordHandlerMap(KeywordHandlerMap *kwHandlerMap);
	void setFunctionParserMap(FunctionParserMap *fncParserMap);
	FormulaVariable *makeFormulaVariable(const std::string &name);
	FormulaElement *makeFormulaVariableOrValue(const std::string &word);
	bool passFunctionArgIfOpen(const std::string &word);
	void insertElement(FormulaElement *formulaElement);
	FormulaElement *getCurrentElement(void) const;
	void insertAsRightHand(FormulaElement *formulaElement);
	void insertAsHand(FormulaElement *formulaElement);
	void makeFormulaElementFromPendingWord(void);
	void addStringValue(const std::string &word);
	FormulaElement *takeFormula(void);
	bool makeFunctionParserIfPendingWordIsFunction(void);

	/**
	 * Get a FormulaElement object at the top of ParenthesisStack, 
	 * which stacks instances of FormulaParenthesis, FormulaFunction, and
	 * its sub classes.
	 *
	 * @retrun A FormulaElement object at the top of ParenthesisStack
	 *         when there is at least one object in the stack.
	 *         If the stack is empty, NULL is returned.
	 */
	FormulaElement *getTopOnParenthesisStack(void) const;

	//
	// SeparatorChecker callbacks
	//
	static void _separatorCbParenthesisOpen
	  (const char separator, SQLFormulaParser *formulaParser);
	virtual void separatorCbParenthesisOpen(const char separator);

	static void _separatorCbParenthesisClose
	  (const char separator, SQLFormulaParser *formulaParser);
	virtual void separatorCbParenthesisClose(const char separator);

	static void _separatorCbQuot
	  (const char separator, SQLFormulaParser *formulaParser);
	virtual void separatorCbQuot(const char separator);

	static void _separatorCbPlus
	  (const char separator, SQLFormulaParser *formulaParser);
	virtual void separatorCbPlus(const char separator);

	static void _separatorCbDiv
	  (const char separator, SQLFormulaParser *formulaParser);
	virtual void separatorCbDiv(const char separator);

	static void _separatorCbEqual
	  (const char separator, SQLFormulaParser *formulaParser);
	virtual void separatorCbEqual(const char separator);

	static void _separatorCbLessThan
	  (const char separator, SQLFormulaParser *formulaParser);
	virtual void separatorCbLessThan(const char separator);

	static void _separatorCbGreaterThan
	  (const char separator, SQLFormulaParser *formulaParser);
	virtual void separatorCbGreaterThan(const char separator);


	//
	// Keyword handlers
	//
	void kwHandlerAnd(void);
	void kwHandlerOr(void);

private:
	static KeywordHandlerMap          m_defaultKeywordHandlerMap;
	static FunctionParserMap          m_defaultFunctionParserMap;

	struct PrivateContext;
	PrivateContext                   *m_ctx;
	FormulaVariableDataGetterFactory  m_columnDataGetterFactory;
	void                             *m_columnDataGetterFactoryPriv;
	mlpl::SeparatorCheckerWithCallback m_separator;
	FormulaElement                   *m_formula;
	bool                              m_hasStatisticalFunc;
	KeywordHandlerMap                *m_keywordHandlerMap;
	FunctionParserMap                *m_functionParserMap;
};

#endif // SQLFormualaParser_h

