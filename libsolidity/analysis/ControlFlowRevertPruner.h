/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0

#pragma once

#include <libsolidity/ast/AST.h>
#include <libsolidity/ast/ASTVisitor.h>
#include <libsolidity/analysis/ControlFlowGraph.h>

#include <libsolutil/Algorithms.h>

namespace solidity::frontend
{

/// Analyses all function flows and removes any CFG nodes that make function
/// calls that will always revert.
class ControlFlowRevertPruner: private ASTConstVisitor
{
public:
	ControlFlowRevertPruner(CFG const& _cfg): m_cfg(_cfg) {}

	void run(ASTNode const& _astRoot);
private:
	// Possible revert states of a function call
	enum class RevertState
	{
		Pending,
		AllPathsRevert,
		HasNonRevertingPath,
	};
	// Enum to make it easier to access tuples
	enum RevertMapConsts { Node = 0, State = 1, Contract = 0, Function = 1 };

	/// Analyze all member functions of the contract
	/// including inherited and overriden functions from base contracts
	bool visit(ContractDefinition const& _contract)
	{
		for (ContractDefinition const* contract: _contract.annotation().linearizedBaseContracts)
			for (FunctionDefinition const* function: contract->definedFunctions())
				removeRevertingPaths(*function, &_contract);

		return false;
	}

	/// Analyze free functions
	bool visit(FunctionDefinition const& _function)
	{
		if (_function.isFree())
			removeRevertingPaths(_function, nullptr);

		return false;
	}

	/// Removes any paths that are still pending after the first iteration
	void removePendingPaths();

	/// Find nodes that did function calls that will revert and remove any exits
	/// from them
	void removeRevertingPaths(FunctionDefinition const& _function, ContractDefinition const* _contract);

	/// Recursively analyze a given function call for reverts, but don't
	/// resolve any recursive loops.
	/// @params _functionCall function call to analyze.
	/// @params _contract Contract from which the call is made.
	/// @returns the revert state of the function call.
	RevertState checkForReverts(FunctionCall const& _functionCall, ContractDefinition const* _contract);

	/// Recursively traverses function flows, analyzing the nodes for possible
	/// reverts.
	/// @param _function function definition whose flow will be traversed.
	/// @param _contract contract that called that function.
	/// @param _flow function flow to be traversed.
	/// @param _onRevertState delegate to modify behavior based on the
	///        revert state of a function call.
	///        If it returns false, processing of that node will stop.
	/// @returns the revert state of the requested function call.
	RevertState traverseFunctionFlow(
		FunctionDefinition const& _function,
		ContractDefinition const* _contract,
		FunctionFlow const& _flow,
		std::function<bool (RevertState, CFGNode*, FunctionFlow const&)> _onRevertState
	);

	/// @returns a key object for the function flow / function revert containers
	/// @param _function function definition to build the key for
	/// @param _contract contract definition from which the call is made
	std::tuple<ContractDefinition const*, FunctionDefinition const*> getNormalizedKey(FunctionDefinition const& _function, ContractDefinition const* _contract);

	// Remembers the states of already processed functions.
	// Anything left pending will be processed in a second step.
	std::map<
		std::tuple<ContractDefinition const*, FunctionDefinition const*>,
		std::tuple<CFGNode*, RevertState>
	> m_functionReverts;

	// Control Flow Graph object.
	CFG const& m_cfg;
};


/// Resolves a function call using
/// - the calling contract
/// - the unresolved function definition
/// - the function call expression
///
class ResolveFunction: private ASTConstVisitor
{
public:
	/// @param _unresolvedFunctionDefinition unresolved function definition to be resolved.
	/// @param _contract Contract from which this function is called.
	ResolveFunction(FunctionDefinition const& _unresolvedFunctionDefinition, ContractDefinition const* _contract):
		m_unresolvedFunctionDefinition(_unresolvedFunctionDefinition),
		m_contract(_contract)
	{}

	/// Resolves a function call.
	/// @param _expression function call expression that will be resolved.
	/// @returns the function definition that will actually called.
	FunctionDefinition const* resolve(Expression const& _expression)
	{
		_expression.accept(*this);
		return m_functionDefinition;
	}
private:
	bool visit(MemberAccess const& _memberAccess) override;
	bool visit(Identifier const& _identifier) override;

private:
	FunctionDefinition const& m_unresolvedFunctionDefinition;
	ContractDefinition const* m_contract;
	FunctionDefinition const* m_functionDefinition = nullptr;
};

}
