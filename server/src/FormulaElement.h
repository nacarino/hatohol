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

#ifndef FormulaElement_h 
#define FormulaElement_h

#include <vector>
#include "ItemDataPtr.h"

// The lower value has high priority
enum FormulaElementPriority {
	FORMULA_ELEM_PRIO_VALUE,
	FORMULA_ELEM_PRIO_VARIABLE,
	FORMULA_ELEM_PRIO_PARENTHESIS,
	FORMULA_ELEM_PRIO_FUNCTION,
	FORMULA_ELEM_PRIO_BETWEEN,
	FORMULA_ELEM_PRIO_IN,
	FORMULA_ELEM_PRIO_EXISTS,
	FORMULA_ELEM_PRIO_IS_NULL,
	FORMULA_ELEM_PRIO_IS_NOT_NULL,
	FORMULA_ELEM_PRIO_DIV,
	FORMULA_ELEM_PRIO_PLUS,
	FORMULA_ELEM_PRIO_GT,
	FORMULA_ELEM_PRIO_GE,
	FORMULA_ELEM_PRIO_CMP_EQ,
	FORMULA_ELEM_PRIO_CMP_NOT_EQ,
	FORMULA_ELEM_PRIO_NOT,
	FORMULA_ELEM_PRIO_AND,
	FORMULA_ELEM_PRIO_OR,
};

enum FormulaOptimizationResultType {
	FORMULA_UNFIXED,
	FORMULA_ALWAYS_CONST,
	FORMULA_ALWAYS_TRUE,
	FORMULA_ALWAYS_FALSE,
};

struct FormulaOptimizationResult {
	FormulaOptimizationResultType type;
	ItemDataPtr                   itemData;

	// constructor
	FormulaOptimizationResult(void);
	FormulaOptimizationResult &operator=(const FormulaOptimizationResult &);
};

// ---------------------------------------------------------------------------
// class: FormulaElement
// ---------------------------------------------------------------------------
class FormulaElement {
public:
	FormulaElement(FormulaElementPriority priority, bool unary = false);
	virtual ~FormulaElement();
	void setLeftHand(FormulaElement *elem);
	void setRightHand(FormulaElement *elem);

	FormulaElement *getLeftHand(void) const;
	FormulaElement *getRightHand(void) const;
	FormulaElement *getParent(void) const;
	FormulaElement *getRootElement(void);
	bool isUnary(void) const;
	bool isTerminalElement(void) const;
	bool priorityOver(FormulaElement *formulaElement);
	bool priorityEqual(FormulaElement *formulaElement);

	/**
	 * Finds the insert point on the branch line in the formula
	 * element tree.
	 *
	 * @param insertionElem A formula element to be inserted.
	 * @param upperLimitElem If \upperLimitElem is not NULL,
	 *                       the search ends at the element whose parent
	 *                       is \upperLimitElem.
	 * @return A formula element whose priority is the lowest in the
	 *         set whose priority is the equal as or higher than
	 *         \insertElem on the branch line
	 *         from  \this instace to the root element.
	 *         If priority of \formulaElemnet is higher than
	 *         that of \this instance, NULL is returned.
	 */
	FormulaElement *findInsertPoint(FormulaElement *insertElem,
	                                FormulaElement *upperLimitElem = NULL);

	virtual FormulaOptimizationResult optimize(void);
	virtual void removeParenthesis(void);
	virtual ItemDataPtr evaluate(void) = 0;
	virtual void resetStatistics(void);

	// mainly for debug
	int getTreeInfo(std::string &str, int maxNumElem = -1, int currNum = 0,
	                int depth = 0);

protected:
	bool getHandDataWithCheck(ItemDataPtr &dataPtr, FormulaElement *hand,
	                          const char *handName);
	bool getLeftHandDataWithCheck(ItemDataPtr &dataPtr);
	bool getRightHandDataWithCheck(ItemDataPtr &dataPtr);
	void setTerminalElement(void);
	virtual void setOptimizationResult(FormulaOptimizationResult &result);
	FormulaOptimizationResult &getOptimizationResult(void);
	virtual std::string getTreeInfoAdditional(void);

private:
	bool             m_unary;
	FormulaElement  *m_leftHand;
	FormulaElement  *m_rightHand;
	FormulaElement  *m_parent;
	FormulaElementPriority m_priority;
	bool             m_terminalElement;
	FormulaOptimizationResult m_optimizationResult;
};

typedef std::vector<FormulaElement *>  FormulaElementVector;
typedef FormulaElementVector::iterator FormulaElementVectorIterator;

// ---------------------------------------------------------------------------
// class: FormulaValue
// ---------------------------------------------------------------------------
class FormulaValue : public FormulaElement {
public:
	FormulaValue(bool val);
	FormulaValue(int number);
	FormulaValue(double number);
	FormulaValue(const std::string &str);
	virtual FormulaOptimizationResult optimize(void);
	virtual ItemDataPtr evaluate(void);

protected:
	virtual std::string getTreeInfoAdditional(void);

private:
	ItemDataPtr m_itemDataPtr;
};

// ---------------------------------------------------------------------------
// class: FormulaVariable
// ---------------------------------------------------------------------------
class FormulaVariable;
class FormulaVariableDataGetter {
public:
	virtual ~FormulaVariableDataGetter() {}
	virtual ItemDataPtr getData(void) = 0;
};

typedef FormulaVariableDataGetter *
(*FormulaVariableDataGetterFactory)(const std::string &name, void *priv);

class FormulaVariable : public FormulaElement {
public:
	FormulaVariable(const std::string &name,
	                FormulaVariableDataGetter *variableDataGetter);
	virtual ~FormulaVariable();
	virtual ItemDataPtr evaluate(void);

	const std::string &getName(void) const;
	FormulaVariableDataGetter *getFormulaVariableGetter(void) const;

protected:
	virtual std::string getTreeInfoAdditional(void);

private:
	std::string                m_name;
	FormulaVariableDataGetter *m_variableGetter;
};

#endif // FormulaElement_h
