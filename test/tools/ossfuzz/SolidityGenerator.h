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
/**
 * Implements generators for synthesizing mostly syntactically valid
 * Solidity test programs.
 */

#pragma once

#include <test/tools/ossfuzz/Generators.h>

#include <liblangutil/Exceptions.h>

#include <memory>
#include <random>
#include <set>
#include <variant>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

namespace solidity::test::fuzzer::mutator
{
/// Forward declarations
class SolidityGenerator;

/// Type declarations
#define SEMICOLON() ;
#define FORWARDDECLAREGENERATORS(G) class G
GENERATORLIST(FORWARDDECLAREGENERATORS, SEMICOLON(), SEMICOLON())
#undef FORWARDDECLAREGENERATORS
#undef SEMICOLON

#define COMMA() ,
using GeneratorPtr = std::variant<
#define VARIANTOFSHARED(G) std::shared_ptr<G>
GENERATORLIST(VARIANTOFSHARED, COMMA(), )
>;
#undef VARIANTOFSHARED
using Generator = std::variant<
#define VARIANTOFGENERATOR(G) G
GENERATORLIST(VARIANTOFGENERATOR, COMMA(), )
>;
#undef VARIANTOFGENERATOR
#undef COMMA
using RandomEngine = std::mt19937_64;
using Distribution = std::uniform_int_distribution<size_t>;

struct UniformRandomDistribution
{
	explicit UniformRandomDistribution(std::unique_ptr<RandomEngine> _randomEngine):
		randomEngine(std::move(_randomEngine))
	{}

	/// @returns an unsigned integer in the range [1, @param _n] chosen
	/// uniformly at random.
	[[nodiscard]] size_t distributionOneToN(size_t _n) const
	{
		solAssert(_n > 0, "");
		return Distribution(1, _n)(*randomEngine);
	}
	/// @returns true with a probability of 1/(@param _n), false otherwise.
	/// @param _n > 1.
	[[nodiscard]] bool probable(size_t _n) const
	{
		solAssert(_n > 1, "");
		return distributionOneToN(_n) == 1;
	}
	/// @returns true with a probability of 1 - 1/(@param _n),
	/// false otherwise.
	/// @param _n > 1.
	[[nodiscard]] bool likely(size_t _n) const
	{
		solAssert(_n > 1, "");
		return !probable(_n);
	}
	/// @returns a subset whose elements are of type @param T
	/// created from the set @param _container using
	/// uniform selection.
	template <typename T>
	std::set<T> subset(std::set<T> const& _container)
	{
		size_t s = _container.size();
		solAssert(s > 1, "");
		std::set<T> subContainer;
		for (auto const& item: _container)
			if (probable(s))
				subContainer.insert(item);
		return subContainer;
	}
	std::unique_ptr<RandomEngine> randomEngine;
};

struct ContractState
{
	explicit ContractState(std::shared_ptr<UniformRandomDistribution> _urd):
		uRandDist(std::move(_urd))
	{}

	std::shared_ptr<UniformRandomDistribution> uRandDist;
};

struct SolidityType
{
	virtual ~SolidityType() = default;
	virtual std::string toString() = 0;
};

struct IntegerType: SolidityType
{
	enum class Bits: size_t
	{
		B8 = 1,
		B16,
		B24,
		B32,
		B40,
		B48,
		B56,
		B64,
		B72,
		B80,
		B88,
		B96,
		B104,
		B112,
		B120,
		B128,
		B136,
		B144,
		B152,
		B160,
		B168,
		B176,
		B184,
		B192,
		B200,
		B208,
		B216,
		B224,
		B232,
		B240,
		B248,
		B256
	};

	IntegerType(
		Bits _bits,
		bool _signed
	):
		signedType(_signed),
		numBits(static_cast<size_t>(_bits) * 8)
	{}
	std::string toString() override
	{
		return (signedType ? "int" : "uint") + std::to_string(numBits);
	}
	bool signedType;
	size_t numBits;
};

struct BoolType: SolidityType
{
	std::string toString() override
	{
		return "bool";
	}
};

struct AddressType: SolidityType
{
	// TODO: Implement address payable
	std::string toString() override
	{
		return "address";
	}
};

struct FixedBytesType: SolidityType
{
	enum class Bytes: size_t
	{
		W1 = 1,
		W2,
		W3,
		W4,
		W5,
		W6,
		W7,
		W8,
		W9,
		W10,
		W11,
		W12,
		W13,
		W14,
		W15,
		W16,
		W17,
		W18,
		W19,
		W20,
		W21,
		W22,
		W23,
		W24,
		W25,
		W26,
		W27,
		W28,
		W29,
		W30,
		W31,
		W32
	};
	FixedBytesType(Bytes _width): numBytes(static_cast<size_t>(_width))
	{}

	std::string toString() override
	{
		return "bytes" + std::to_string(numBytes);
	}
	size_t numBytes;
};

struct BytesType: SolidityType
{
	std::string toString() override
	{
		return "bytes memory";
	}
};

struct ContractType: SolidityType
{
	ContractType(std::string _name): contractName(_name)
	{}
	std::string toString() override
	{
		return name();
	}
	std::string name()
	{
		return contractName;
	}
	std::string contractName;
};

struct FunctionType: SolidityType
{
	FunctionType() = default;
	~FunctionType() override
	{
		inputs.clear();
		outputs.clear();
	}

	void addInput(std::shared_ptr<SolidityType> _input)
	{
		inputs.emplace_back(_input);
	}

	void addOutput(std::shared_ptr<SolidityType> _output)
	{
		outputs.emplace_back(_output);
	}

	std::string toString() override
	{
		auto typeString = [](std::vector<std::shared_ptr<SolidityType>>& _types)
		{
			std::string sep;
			std::string typeStr;
			for (auto const& i: _types)
			{
				typeStr += sep + i->toString();
				if (sep.empty())
					sep = ",";
			}
			return typeStr;
		};

		std::string retString = std::string("function ") + "(" + typeString(inputs) + ")";
		if (outputs.empty())
			return retString + " public pure";
		else
			return retString + " public pure returns (" + typeString(outputs) +	")";
	}

	std::vector<std::shared_ptr<SolidityType>> inputs;
	std::vector<std::shared_ptr<SolidityType>> outputs;
};

/// Forward declaration
struct TestState;

struct SourceState
{
	explicit SourceState(std::shared_ptr<UniformRandomDistribution> _urd):
		uRandDist(std::move(_urd)),
		importedSources({})
	{}
	void addFreeFunction(std::string& _functionName)
	{
		exports[std::dynamic_pointer_cast<SolidityType>(std::make_shared<FunctionType>())] = _functionName;
	}
	bool freeFunction(std::string const& _functionName)
	{
		return !(exports | ranges::views::filter([&_functionName](auto& _p) { return _p.second == _functionName; })).empty();
	}
	bool contractType()
	{
		return !(exports | ranges::views::filter([](auto& _i) {
			return bool(std::dynamic_pointer_cast<ContractType>(_i.first));
		})).empty();
	}
	std::string randomContract()
	{
		auto contracts = exports |
			ranges::views::filter([](auto& _item) -> bool {
				return bool(std::dynamic_pointer_cast<ContractType>(_item.first));
			}) |
			ranges::views::transform([](auto& _item) -> std::string {
				return _item.second;
			}) | ranges::to<std::vector<std::string>>;
		return contracts[uRandDist->distributionOneToN(contracts.size()) - 1];
	}
	std::shared_ptr<SolidityType> randomContractType()
	{
		auto contracts = exports |
			ranges::views::filter([](auto& _item) -> bool {
				return bool(std::dynamic_pointer_cast<ContractType>(_item.first));
			}) |
			ranges::views::transform([](auto& _item) -> std::shared_ptr<SolidityType> {
				return _item.first;
			}) |
			ranges::to<std::vector<std::shared_ptr<SolidityType>>>;
		return contracts[uRandDist->distributionOneToN(contracts.size()) - 1];
	}
	void addImportedSourcePath(std::string& _sourcePath)
	{
		importedSources.emplace(_sourcePath);
	}
	void resolveImports(std::map<std::shared_ptr<SolidityType>, std::string> _imports)
	{
		for (auto const& item: _imports)
			exports.emplace(item);
	}
	[[nodiscard]] bool sourcePathImported(std::string const& _sourcePath) const
	{
		return importedSources.count(_sourcePath);
	}
	~SourceState()
	{
		importedSources.clear();
	}
	/// Prints source state to @param _os.
	void print(std::ostream& _os) const;
	std::shared_ptr<UniformRandomDistribution> uRandDist;
	std::set<std::string> importedSources;
	std::map<std::shared_ptr<SolidityType>, std::string> exports;
};

struct FunctionState
{
	FunctionState() = default;
	~FunctionState()
	{
		inputs.clear();
		outputs.clear();
	}
	void addInput(std::pair<std::string, std::shared_ptr<SolidityType>> _input)
	{
		inputs.emplace(_input);
	}
	void addOutput(std::pair<std::string, std::shared_ptr<SolidityType>> _output)
	{
		outputs.emplace(_output);
	}

	std::map<std::string, std::shared_ptr<SolidityType>> inputs;
	std::map<std::string, std::shared_ptr<SolidityType>> outputs;
};

struct TestState
{
	explicit TestState(std::shared_ptr<UniformRandomDistribution> _urd):
		sourceUnitState({}),
		contractState({}),
		currentSourceUnitPath({}),
		currentContract({}),
		currentFunction({}),
		uRandDist(std::move(_urd)),
		numSourceUnits(0),
		numContracts(0),
		numFunctions(0),
		indentationLevel(0)
	{}
	/// Adds @param _path to @name sourceUnitPaths updates
	/// @name currentSourceUnitPath.
	void addSourceUnit(std::string const& _path)
	{
		sourceUnitState.emplace(_path, std::make_shared<SourceState>(uRandDist));
		currentSourceUnitPath = _path;
	}
	/// Adds @param _name to @name contractState updates
	/// @name currentContract.
	void addContract(std::string const& _name)
	{
		contractState.emplace(_name, std::make_shared<ContractState>(uRandDist));
		sourceUnitState[currentSourceUnitPath]->exports[
			std::make_shared<ContractType>(_name)
	    ] = _name;
		currentContract = _name;
	}
	void addFunction(std::string const& _name)
	{
		functionState.emplace(_name, std::make_shared<FunctionState>());
		currentFunction = _name;
	}
	std::shared_ptr<FunctionState> currentFunctionState()
	{
		return functionState[currentFunction];
	}
	/// Returns true if @name sourceUnitPaths is empty,
	/// false otherwise.
	[[nodiscard]] bool empty() const
	{
		return sourceUnitState.empty();
	}
	/// Returns the number of items in @name sourceUnitPaths.
	[[nodiscard]] size_t size() const
	{
		return sourceUnitState.size();
	}
	/// Returns a new source path name that is formed by concatenating
	/// a static prefix @name m_sourceUnitNamePrefix, a monotonically
	/// increasing counter starting from 0 and the postfix (extension)
	/// ".sol".
	[[nodiscard]] std::string newPath() const
	{
		return sourceUnitNamePrefix + std::to_string(numSourceUnits) + ".sol";
	}
	[[nodiscard]] std::string newContract() const
	{
		return contractPrefix + std::to_string(numContracts);
	}
	[[nodiscard]] std::string newFunction() const
	{
		return functionPrefix + std::to_string(numFunctions);
	}
	[[nodiscard]] std::string currentPath() const
	{
		solAssert(numSourceUnits > 0, "");
		return currentSourceUnitPath;
	}
	/// Adds @param _path to list of source paths in global test
	/// state and increments @name m_numSourceUnits.
	void updateSourcePath(std::string const& _path)
	{
		addSourceUnit(_path);
		numSourceUnits++;
	}
	/// Adds @param _contract to list of contracts in global test state and
	/// increments @name numContracts
	void updateContract(std::string const& _name)
	{
		addContract(_name);
		numContracts++;
	}
	void updateFunction(std::string const& _name)
	{
		addFunction(_name);
		numFunctions++;
	}
	void addSource()
	{
		updateSourcePath(newPath());
	}
	/// Increments indentation level globally.
	void indent()
	{
		++indentationLevel;
	}
	/// Decrements indentation level globally.
	void unindent()
	{
		--indentationLevel;
	}
	~TestState()
	{
		sourceUnitState.clear();
		contractState.clear();
	}
	/// Prints test state to @param _os.
	void print(std::ostream& _os) const;
	/// Returns a randomly chosen path from @param _sourceUnitPaths.
	[[nodiscard]] std::string randomPath(std::set<std::string> const& _sourceUnitPaths) const;
	[[nodiscard]] std::set<std::string> sourceUnitPaths() const;
	/// Returns a randomly chosen path from @name sourceUnitPaths.
	[[nodiscard]] std::string randomPath() const;
	/// Returns a randomly chosen non current source unit path.
	[[nodiscard]] std::string randomNonCurrentPath() const;
	/// Map of source name -> state
	std::map<std::string, std::shared_ptr<SourceState>> sourceUnitState;
	/// Map of contract name -> state
	std::map<std::string, std::shared_ptr<ContractState>> contractState;
	/// Map of function name -> state
	std::map<std::string, std::shared_ptr<FunctionState>> functionState;
	/// Source path being currently visited.
	std::string currentSourceUnitPath;
	/// Current contract
	std::string currentContract;
	/// Current function
	std::string currentFunction;
	/// Uniform random distribution.
	std::shared_ptr<UniformRandomDistribution> uRandDist;
	/// Number of source units in test input
	size_t numSourceUnits;
	/// Number of contracts in test input
	size_t numContracts;
	/// Number of functions in test input
	size_t numFunctions;
	/// Indentation level
	unsigned indentationLevel;
	/// Source name prefix
	std::string const sourceUnitNamePrefix = "su";
	/// Contract name prefix
	std::string const contractPrefix = "C";
	/// Function name prefix
	std::string const functionPrefix = "f";
};

struct TypeGenerator
{
	TypeGenerator(std::shared_ptr<TestState> _state): state(std::move(_state))
	{}

	enum class Type: size_t
	{
		INTEGER = 1,
		BOOL,
		FIXEDBYTES,
		BYTES,
		ADDRESS,
		FUNCTION,
		CONTRACT,
		TYPEMAX
	};

	std::shared_ptr<SolidityType> type();

	Type typeCategory()
	{
		return static_cast<Type>(state->uRandDist->distributionOneToN(static_cast<size_t>(Type::TYPEMAX) - 1));
	}

	std::shared_ptr<TestState> state;
};

struct GeneratorBase
{
	explicit GeneratorBase(std::shared_ptr<SolidityGenerator> _mutator);
	template <typename T>
	std::shared_ptr<T> generator()
	{
		for (auto& g: generators)
			if (std::holds_alternative<std::shared_ptr<T>>(g.first))
				return std::get<std::shared_ptr<T>>(g.first);
		solAssert(false, "");
	}
	/// @returns test fragment created by this generator.
	std::string generate()
	{
		std::string generatedCode = visit();
		endVisit();
		return generatedCode;
	}
	/// @returns indentation as string. Each indentation level comprises two
	/// whitespace characters.
	std::string indentation(unsigned _indentationLevel)
	{
		return std::string(_indentationLevel * 2, ' ');
	}
	/// @returns a string representing the generation of
	/// the Solidity grammar element.
	virtual std::string visit() = 0;
	/// Method called after visiting this generator. Used
	/// for clearing state if necessary.
	virtual void endVisit() {}
	/// Visitor that invokes child grammar elements of
	/// this grammar element returning their string
	/// representations.
	std::string visitChildren();
	/// Adds generators for child grammar elements of
	/// this grammar element.
	void addGenerators(std::set<std::pair<GeneratorPtr, unsigned>> _generators)
	{
		generators += _generators;
	}
	/// Virtual method to obtain string name of generator.
	virtual std::string name() = 0;
	/// Virtual method to add generators that this grammar
	/// element depends on. If not overridden, there are
	/// no dependencies.
	virtual void setup() {}
	virtual ~GeneratorBase()
	{
		generators.clear();
	}
	/// Shared pointer to the mutator instance
	std::shared_ptr<SolidityGenerator> mutator;
	/// Set of generators used by this generator.
	std::set<std::pair<GeneratorPtr, unsigned>> generators;
	/// Shared ptr to global test state.
	std::shared_ptr<TestState> state;
	/// Uniform random distribution
	std::shared_ptr<UniformRandomDistribution> uRandDist;
};

class TestCaseGenerator: public GeneratorBase
{
public:
	explicit TestCaseGenerator(std::shared_ptr<SolidityGenerator> _mutator):
		GeneratorBase(std::move(_mutator))
	{}
	void setup() override;
	std::string visit() override;
	std::string name() override
	{
		return "Test case generator";
	}
private:
	/// @returns a new source path name that is formed by concatenating
	/// a static prefix @name m_sourceUnitNamePrefix, a monotonically
	/// increasing counter starting from 0 and the postfix (extension)
	/// ".sol".
	[[nodiscard]] std::string path() const
	{
		return m_sourceUnitNamePrefix + std::to_string(m_numSourceUnits) + ".sol";
	}
	/// Adds @param _path to list of source paths in global test
	/// state and increments @name m_numSourceUnits.
	void updateSourcePath(std::string const& _path)
	{
		state->addSourceUnit(_path);
		m_numSourceUnits++;
	}
	/// Number of source units in test input
	size_t m_numSourceUnits;
	/// String prefix of source unit names
	std::string const m_sourceUnitNamePrefix = "su";
	/// Maximum number of source units per test input
	static constexpr unsigned s_maxSourceUnits = 3;
};

class SourceUnitGenerator: public GeneratorBase
{
public:
	explicit SourceUnitGenerator(std::shared_ptr<SolidityGenerator> _mutator):
		GeneratorBase(std::move(_mutator))
	{}
	void setup() override;
	std::string visit() override;
	std::string name() override { return "Source unit generator"; }
private:
	static unsigned constexpr s_maxImports = 2;
	static unsigned constexpr s_maxFreeFunctions = 2;
};

class PragmaGenerator: public GeneratorBase
{
public:
	explicit PragmaGenerator(std::shared_ptr<SolidityGenerator> _mutator):
		GeneratorBase(std::move(_mutator))
	{}
	std::string visit() override;
	std::string name() override { return "Pragma generator"; }
private:
	std::set<std::string> const s_genericPragmas = {
		R"(pragma solidity >= 0.0.0;)",
		R"(pragma experimental SMTChecker;)",
	};
	std::vector<std::string> const s_abiPragmas = {
		R"(pragma abicoder v1;)",
		R"(pragma abicoder v2;)"
	};
};

class ImportGenerator: public GeneratorBase
{
public:
	explicit ImportGenerator(std::shared_ptr<SolidityGenerator> _mutator):
	       GeneratorBase(std::move(_mutator))
	{}
	std::string visit() override;
	std::string name() override { return "Import generator"; }
};

class ContractGenerator: public GeneratorBase
{
public:
	explicit ContractGenerator(std::shared_ptr<SolidityGenerator> _mutator):
		GeneratorBase(std::move(_mutator))
	{}
	void setup() override;
	std::string visit() override;
	std::string name() override { return "Contract generator"; }
private:
	static unsigned constexpr s_maxFunctions = 4;
};

class FunctionGenerator: public GeneratorBase
{
public:
	explicit FunctionGenerator(std::shared_ptr<SolidityGenerator> _mutator):
		GeneratorBase(std::move(_mutator)),
		m_freeFunction(true)
	{}
	std::string visit() override;
	std::string name() override { return "Function generator"; }
	/// Sets @name m_freeFunction to @param _freeFunction.
	void scope(bool _freeFunction)
	{
		m_freeFunction = _freeFunction;
	}
private:
	bool m_freeFunction;

};

class SolidityGenerator: public std::enable_shared_from_this<SolidityGenerator>
{
public:
	explicit SolidityGenerator(unsigned _seed);

	/// @returns the generator of type @param T.
	template <typename T>
	std::shared_ptr<T> generator();
	/// @returns a shared ptr to underlying random
	/// number distribution.
	std::shared_ptr<UniformRandomDistribution> uniformRandomDist()
	{
		return m_urd;
	}
	/// @returns a pseudo randomly generated test case.
	std::string generateTestProgram();
	/// @returns shared ptr to global test state.
	std::shared_ptr<TestState> testState()
	{
		return m_state;
	}
private:
	template <typename T>
	void createGenerator()
	{
		m_generators.insert(
			std::make_shared<T>(shared_from_this())
		);
	}
	template <std::size_t I = 0>
	void createGenerators();
	void destroyGenerators()
	{
		m_generators.clear();
	}
	/// Sub generators
	std::set<GeneratorPtr> m_generators;
	/// Shared global test state
	std::shared_ptr<TestState> m_state;
	/// Uniform random distribution
	std::shared_ptr<UniformRandomDistribution> m_urd;
};
}
