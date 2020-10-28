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

#include <libsolidity/analysis/ControlFlowRevertPruner.h>

#include <libsolutil/Algorithms.h>

namespace solidity::frontend
{

void ControlFlowRevertPruner::run(ASTNode const& _astRoot)
{
	// Run first iteration without following recursive loops
	_astRoot.accept(*this);

	// In the second iteration we process anything left pending
	removePendingPaths();
}

void ControlFlowRevertPruner::removePendingPaths()
{
	// Anything still left pending must be a infinite recursive loop.
	for (auto& [key, val]: m_functionReverts)
	{
		RevertState& revertState = std::get<State>(val);
		auto functionFlow = m_cfg.functionFlow(
			*std::get<Function>(key),
			std::get<Contract>(key)
		);

		if (CFGNode* node = std::get<Node>(val))
			for (auto const* functionCall: node->functionCalls)
				switch (checkForReverts(*functionCall, std::get<Contract>(key)))
				{
				// Remove exits & add revert node
				case RevertState::AllPathsRevert:
					[[fallthrough]];
				case RevertState::Pending:
					node->exits.clear();
					node->exits.push_back(functionFlow.revert);
					break;
				case RevertState::HasNonRevertingPath:
					break;
				}

		if (revertState == RevertState::Pending)
			revertState = RevertState::AllPathsRevert;
	}
}

void ControlFlowRevertPruner::removeRevertingPaths(FunctionDefinition const& _function, ContractDefinition const* _contract)
{
	auto& functionFlow = m_cfg.functionFlow(_function, _contract);
	std::tuple const revertMapKey = {_contract, &_function};

	std::get<State>(m_functionReverts[revertMapKey]) = RevertState::Pending;

	// Traverse the function flow and remove exits from nodes with reverting
	// function calls.
	// Recursive loops are saved for later processing.
	traverseFunctionFlow(_function, _contract, functionFlow,
		[&](RevertState _state, CFGNode* _node, FunctionFlow const& _flow) {
			if (_state == RevertState::AllPathsRevert)
			{
				// Remove exits if call always reverts
				_node->exits.clear();
				_node->exits.push_back(_flow.revert);
			}
			else if (_state == RevertState::Pending)
			{
				// Remember node for later re-evaluation
				std::get<Node>(m_functionReverts[revertMapKey]) = _node;
			}

			return true;
	});
}
ControlFlowRevertPruner::RevertState ControlFlowRevertPruner::checkForReverts(FunctionCall const& _functionCall, ContractDefinition const* _contract)
{
	auto const& functionType = dynamic_cast<FunctionType const&>(
		*_functionCall.expression().annotation().type
	);

	if (!functionType.hasDeclaration()) // TODO assert?
		return RevertState::HasNonRevertingPath;

	auto const& unresolvedFunctionDefinition =
		dynamic_cast<FunctionDefinition const&>(functionType.declaration());

	auto const* functionDefinition = ResolveFunction(unresolvedFunctionDefinition, _contract).resolve(_functionCall.expression());
	solAssert(functionDefinition != nullptr, "");

	auto revertMapKey = getNormalizedKey(*functionDefinition, _contract);
	auto reverts = m_functionReverts.find(revertMapKey);

	if (reverts != m_functionReverts.end())
		return std::get<State>(reverts->second);

	RevertState& revertState = std::get<State>(m_functionReverts[revertMapKey]);
	revertState = RevertState::AllPathsRevert;

	FunctionFlow const& funcFlow = m_cfg.functionFlow(*functionDefinition, std::get<0>(revertMapKey));

	// Traverse function flow, skip paths with nodes that have reverting
	// function calls
	return traverseFunctionFlow(*functionDefinition, _contract, funcFlow,
		[&](RevertState _state, CFGNode*, FunctionFlow const&) {
			if (_state == RevertState::AllPathsRevert)
				// Stop processing this node if all paths revert
				return false;
			else if (_state == RevertState::Pending)
				// Set to pending, but will be set again if we find a
				// non-pending exit (a single non-pending exit is enough to
				// set HasNonRevertingPath)
				revertState = RevertState::Pending;

			return true;
	});
}

ControlFlowRevertPruner::RevertState ControlFlowRevertPruner::traverseFunctionFlow(
	FunctionDefinition const& _function,
	ContractDefinition const* _contract,
	FunctionFlow const& _flow,
	std::function<bool (RevertState, CFGNode*, FunctionFlow const&)> _onRevertState
)
{
	auto& revertState = std::get<State>(m_functionReverts[getNormalizedKey(_function, _contract)]);

	// Track pending nodes to detect non-pending exits
	std::set<CFGNode*> pendingNodes;

	solidity::util::BreadthFirstSearch<CFGNode*>{{_flow.entry}}.run(
		[&](CFGNode* _node, auto&& _addChild) {
			bool pending = pendingNodes.find(_node) != pendingNodes.end();

			if (_node == _flow.exit)
			{
				// Found a non-pending exit? Update our state then.
				if (!pending)
					revertState = RevertState::HasNonRevertingPath;

				return;
			}

			for (auto const* functionCall: _node->functionCalls)
			{
				auto reverts = checkForReverts(*functionCall, _contract);

				if (!_onRevertState(reverts, _node, _flow))
					return;

				if (reverts == RevertState::Pending)
				{
					pending = true;
					pendingNodes.insert(_node);
				}
			}

			for (CFGNode* exit: _node->exits)
			{
				_addChild(exit);
				if (pending)
					pendingNodes.insert(exit);
			}
	});

	return revertState;
}

std::tuple<ContractDefinition const*, FunctionDefinition const*> ControlFlowRevertPruner::getNormalizedKey(FunctionDefinition const& _function, ContractDefinition const* _contract)
{
	ContractDefinition const* keyContract = _contract;

	if (_function.isFree())
		keyContract = nullptr;
	else if (_function.annotation().contract->isLibrary())
		keyContract = _function.annotation().contract;

	return std::tuple{keyContract, &_function};
}

bool ResolveFunction::visit(MemberAccess const& _memberAccess)
{
	if (*_memberAccess.annotation().requiredLookup == VirtualLookup::Super)
	{
		if (auto const typeType = dynamic_cast<TypeType const*>(_memberAccess.expression().annotation().type))
			if (auto const contractType = dynamic_cast<ContractType const*>(typeType->actualType()))
			{
				solAssert(contractType->isSuper(), "");
				ContractDefinition const* superContract = contractType->contractDefinition().superContract(*m_contract);

				m_functionDefinition =
					&m_unresolvedFunctionDefinition.resolveVirtual(
						*m_contract,
						superContract
					);
			}
	}
	else
	{
		solAssert(*_memberAccess.annotation().requiredLookup == VirtualLookup::Static, "");
		m_functionDefinition = &m_unresolvedFunctionDefinition;
	}
	return false;
}

bool ResolveFunction::visit(Identifier const& _identifier)
{
	solAssert(*_identifier.annotation().requiredLookup == VirtualLookup::Virtual, "");
	m_functionDefinition = &m_unresolvedFunctionDefinition.resolveVirtual(*m_contract);
	return false;
}

}
