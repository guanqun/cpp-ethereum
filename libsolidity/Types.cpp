/*
    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @author Christian <c@ethdev.com>
 * @date 2014
 * Solidity data types
 */

#include <libdevcore/CommonIO.h>
#include <libdevcore/CommonData.h>
#include <libsolidity/Utils.h>
#include <libsolidity/Types.h>
#include <libsolidity/AST.h>

using namespace std;

namespace dev
{
namespace solidity
{

shared_ptr<Type const> Type::fromElementaryTypeName(Token::Value _typeToken)
{
	solAssert(Token::isElementaryTypeName(_typeToken), "Elementary type name expected.");

	if (Token::INT <= _typeToken && _typeToken <= Token::HASH256)
	{
		int offset = _typeToken - Token::INT;
		int bytes = offset % 33;
		if (bytes == 0)
			bytes = 32;
		int modifier = offset / 33;
		return make_shared<IntegerType>(bytes * 8,
										modifier == 0 ? IntegerType::Modifier::SIGNED :
										modifier == 1 ? IntegerType::Modifier::UNSIGNED :
										IntegerType::Modifier::HASH);
	}
	else if (_typeToken == Token::ADDRESS)
		return make_shared<IntegerType>(0, IntegerType::Modifier::ADDRESS);
	else if (_typeToken == Token::BOOL)
		return make_shared<BoolType>();
	else if (Token::STRING0 <= _typeToken && _typeToken <= Token::STRING32)
		return make_shared<StaticStringType>(int(_typeToken) - int(Token::STRING0));
	else
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unable to convert elementary typename " +
																		 std::string(Token::toString(_typeToken)) + " to type."));
}

shared_ptr<Type const> Type::fromUserDefinedTypeName(UserDefinedTypeName const& _typeName)
{
	Declaration const* declaration = _typeName.getReferencedDeclaration();
	if (StructDefinition const* structDef = dynamic_cast<StructDefinition const*>(declaration))
		return make_shared<StructType>(*structDef);
	else if (FunctionDefinition const* function = dynamic_cast<FunctionDefinition const*>(declaration))
		return make_shared<FunctionType>(*function);
	else if (ContractDefinition const* contract = dynamic_cast<ContractDefinition const*>(declaration))
		return make_shared<ContractType>(*contract);
	return shared_ptr<Type const>();
}

shared_ptr<Type const> Type::fromMapping(Mapping const& _typeName)
{
	shared_ptr<Type const> keyType = _typeName.getKeyType().toType();
	if (!keyType)
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Error resolving type name."));
	shared_ptr<Type const> valueType = _typeName.getValueType().toType();
	if (!valueType)
		BOOST_THROW_EXCEPTION(_typeName.getValueType().createTypeError("Invalid type name"));
	return make_shared<MappingType>(keyType, valueType);
}

shared_ptr<Type const> Type::forLiteral(Literal const& _literal)
{
	switch (_literal.getToken())
	{
	case Token::TRUE_LITERAL:
	case Token::FALSE_LITERAL:
		return make_shared<BoolType>();
	case Token::NUMBER:
		return IntegerConstantType::fromLiteral(_literal.getValue());
	case Token::STRING_LITERAL:
		//@todo put larger strings into dynamic strings
		return StaticStringType::smallestTypeForLiteral(_literal.getValue());
	default:
		return shared_ptr<Type>();
	}
}

TypePointer Type::commonType(TypePointer const& _a, TypePointer const& _b)
{
	if (_b->isImplicitlyConvertibleTo(*_a))
		return _a;
	else if (_a->isImplicitlyConvertibleTo(*_b))
		return _b;
	else
		return TypePointer();
}

const MemberList Type::EmptyMemberList = MemberList();

IntegerType::IntegerType(int _bits, IntegerType::Modifier _modifier):
	m_bits(_bits), m_modifier(_modifier)
{
	if (isAddress())
		m_bits = 160;
	solAssert(m_bits > 0 && m_bits <= 256 && m_bits % 8 == 0,
			  "Invalid bit number for integer type: " + dev::toString(_bits));
}

bool IntegerType::isImplicitlyConvertibleTo(Type const& _convertTo) const
{
	if (_convertTo.getCategory() != getCategory())
		return false;
	IntegerType const& convertTo = dynamic_cast<IntegerType const&>(_convertTo);
	if (convertTo.m_bits < m_bits)
		return false;
	if (isAddress())
		return convertTo.isAddress();
	else if (isHash())
		return convertTo.isHash();
	else if (isSigned())
		return convertTo.isSigned();
	else
		return !convertTo.isSigned() || convertTo.m_bits > m_bits;
}

bool IntegerType::isExplicitlyConvertibleTo(Type const& _convertTo) const
{
	if (_convertTo.getCategory() == Category::STRING)
	{
		StaticStringType const& convertTo = dynamic_cast<StaticStringType const&>(_convertTo);
		return isHash() && (m_bits == convertTo.getNumBytes() * 8);
	}
	return _convertTo.getCategory() == getCategory() || _convertTo.getCategory() == Category::CONTRACT;
}

TypePointer IntegerType::unaryOperatorResult(Token::Value _operator) const
{
	// "delete" is ok for all integer types
	if (_operator == Token::DELETE)
		return make_shared<VoidType>();
	// no further unary operators for addresses
	else if (isAddress())
		return TypePointer();
	// "~" is ok for all other types
	else if (_operator == Token::BIT_NOT)
		return shared_from_this();
	// nothing else for hashes
	else if (isHash())
		return TypePointer();
	// for non-hash integers, we allow +, -, ++ and --
	else if (_operator == Token::ADD || _operator == Token::SUB ||
			_operator == Token::INC || _operator == Token::DEC)
		return shared_from_this();
	else
		return TypePointer();
}

bool IntegerType::operator==(Type const& _other) const
{
	if (_other.getCategory() != getCategory())
		return false;
	IntegerType const& other = dynamic_cast<IntegerType const&>(_other);
	return other.m_bits == m_bits && other.m_modifier == m_modifier;
}

string IntegerType::toString() const
{
	if (isAddress())
		return "address";
	string prefix = isHash() ? "hash" : (isSigned() ? "int" : "uint");
	return prefix + dev::toString(m_bits);
}

TypePointer IntegerType::binaryOperatorResult(Token::Value _operator, TypePointer const& _other) const
{
	if (_other->getCategory() != Category::INTEGER_CONSTANT && _other->getCategory() != getCategory())
		return TypePointer();
	auto commonType = dynamic_pointer_cast<IntegerType const>(Type::commonType(shared_from_this(), _other));

	if (!commonType)
		return TypePointer();

	// All integer types can be compared
	if (Token::isCompareOp(_operator))
		return commonType;

	// Nothing else can be done with addresses, but hashes can receive bit operators
	if (commonType->isAddress())
		return TypePointer();
	else if (commonType->isHash() && !Token::isBitOp(_operator))
		return TypePointer();
	else
		return commonType;
}

const MemberList IntegerType::AddressMemberList =
	MemberList({{"balance", make_shared<IntegerType >(256)},
				{"callstring32", make_shared<FunctionType>(strings{"string32"}, strings{},
														   FunctionType::Location::BARE)},
				{"callstring32string32", make_shared<FunctionType>(strings{"string32", "string32"},
																   strings{}, FunctionType::Location::BARE)},
				{"send", make_shared<FunctionType>(strings{"uint"}, strings{}, FunctionType::Location::SEND)}});

shared_ptr<IntegerConstantType const> IntegerConstantType::fromLiteral(string const& _literal)
{
	return make_shared<IntegerConstantType>(bigint(_literal));
}

bool IntegerConstantType::isImplicitlyConvertibleTo(Type const& _convertTo) const
{
	TypePointer integerType = getIntegerType();
	return integerType && integerType->isImplicitlyConvertibleTo(_convertTo);
}

bool IntegerConstantType::isExplicitlyConvertibleTo(Type const& _convertTo) const
{
	TypePointer integerType = getIntegerType();
	return integerType && integerType->isExplicitlyConvertibleTo(_convertTo);
}

TypePointer IntegerConstantType::unaryOperatorResult(Token::Value _operator) const
{
	bigint value;
	switch (_operator)
	{
	case Token::BIT_NOT:
		value = ~m_value;
		break;
	case Token::ADD:
		value = m_value;
		break;
	case Token::SUB:
		value = -m_value;
		break;
	default:
		return TypePointer();
	}
	return make_shared<IntegerConstantType>(value);
}

TypePointer IntegerConstantType::binaryOperatorResult(Token::Value _operator, TypePointer const& _other) const
{
	if (_other->getCategory() == Category::INTEGER)
	{
		shared_ptr<IntegerType const> integerType = getIntegerType();
		if (!integerType)
			return TypePointer();
		return integerType->binaryOperatorResult(_operator, _other);
	}
	else if (_other->getCategory() != getCategory())
		return TypePointer();

	IntegerConstantType const& other = dynamic_cast<IntegerConstantType const&>(*_other);
	if (Token::isCompareOp(_operator))
	{
		shared_ptr<IntegerType const> thisIntegerType = getIntegerType();
		shared_ptr<IntegerType const> otherIntegerType = other.getIntegerType();
		if (!thisIntegerType || !otherIntegerType)
			return TypePointer();
		return thisIntegerType->binaryOperatorResult(_operator, otherIntegerType);
	}
	else
	{
		bigint value;
		switch (_operator)
		{
		case Token::BIT_OR:
			value = m_value | other.m_value;
			break;
		case Token::BIT_XOR:
			value = m_value ^ other.m_value;
			break;
		case Token::BIT_AND:
			value = m_value & other.m_value;
			break;
		case Token::ADD:
			value = m_value + other.m_value;
			break;
		case Token::SUB:
			value = m_value - other.m_value;
			break;
		case Token::MUL:
			value = m_value * other.m_value;
			break;
		case Token::DIV:
			if (other.m_value == 0)
				return TypePointer();
			value = m_value / other.m_value;
			break;
		case Token::MOD:
			if (other.m_value == 0)
				return TypePointer();
			value = m_value % other.m_value;
			break;
		default:
			return TypePointer();
		}
		return make_shared<IntegerConstantType>(value);
	}
}

bool IntegerConstantType::operator==(Type const& _other) const
{
	if (_other.getCategory() != getCategory())
		return false;
	return m_value == dynamic_cast<IntegerConstantType const&>(_other).m_value;
}

string IntegerConstantType::toString() const
{
	return "int_const " + m_value.str();
}

u256 IntegerConstantType::literalValue(Literal const*) const
{
	// we ignore the literal and hope that the type was correctly determined
	solAssert(m_value <= u256(-1), "Integer constant too large.");
	solAssert(m_value >= -(bigint(1) << 255), "Integer constant too small.");
	if (m_value >= 0)
		return u256(m_value);
	else
		return s2u(s256(m_value));
}

shared_ptr<IntegerType const> IntegerConstantType::getIntegerType() const
{
	bigint value = m_value;
	bool negative = (value < 0);
	if (negative) // convert to positive number of same bit requirements
		value = ((-value) - 1) << 1;
	if (value > u256(-1))
		return shared_ptr<IntegerType const>();
	else
		return make_shared<IntegerType>(max(bytesRequired(value), 1u) * 8,
										negative ? IntegerType::Modifier::SIGNED
												 : IntegerType::Modifier::UNSIGNED);
}

shared_ptr<StaticStringType> StaticStringType::smallestTypeForLiteral(string const& _literal)
{
	if (_literal.length() <= 32)
		return make_shared<StaticStringType>(_literal.length());
	return shared_ptr<StaticStringType>();
}

StaticStringType::StaticStringType(int _bytes): m_bytes(_bytes)
{
	solAssert(m_bytes >= 0 && m_bytes <= 32,
			  "Invalid byte number for static string type: " + dev::toString(m_bytes));
}

bool StaticStringType::isImplicitlyConvertibleTo(Type const& _convertTo) const
{
	if (_convertTo.getCategory() != getCategory())
		return false;
	StaticStringType const& convertTo = dynamic_cast<StaticStringType const&>(_convertTo);
	return convertTo.m_bytes >= m_bytes;
}

bool StaticStringType::isExplicitlyConvertibleTo(Type const& _convertTo) const
{
	if (_convertTo.getCategory() == Category::INTEGER)
	{
		IntegerType const& convertTo = dynamic_cast<IntegerType const&>(_convertTo);
		if (convertTo.isHash() && (m_bytes * 8 == convertTo.getNumBits()))
			return true;
	}
	return isImplicitlyConvertibleTo(_convertTo);
}

bool StaticStringType::operator==(Type const& _other) const
{
	if (_other.getCategory() != getCategory())
		return false;
	StaticStringType const& other = dynamic_cast<StaticStringType const&>(_other);
	return other.m_bytes == m_bytes;
}

u256 StaticStringType::literalValue(const Literal* _literal) const
{
	solAssert(_literal, "");
	u256 value = 0;
	for (char c: _literal->getValue())
		value = (value << 8) | byte(c);
	return value << ((32 - _literal->getValue().length()) * 8);
}

bool BoolType::isExplicitlyConvertibleTo(Type const& _convertTo) const
{
	// conversion to integer is fine, but not to address
	// this is an example of explicit conversions being not transitive (though implicit should be)
	if (_convertTo.getCategory() == getCategory())
	{
		IntegerType const& convertTo = dynamic_cast<IntegerType const&>(_convertTo);
		if (!convertTo.isAddress())
			return true;
	}
	return isImplicitlyConvertibleTo(_convertTo);
}

u256 BoolType::literalValue(Literal const* _literal) const
{
	solAssert(_literal, "");
	if (_literal->getToken() == Token::TRUE_LITERAL)
		return u256(1);
	else if (_literal->getToken() == Token::FALSE_LITERAL)
		return u256(0);
	else
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Bool type constructed from non-boolean literal."));
}

TypePointer BoolType::unaryOperatorResult(Token::Value _operator) const
{
	if (_operator == Token::DELETE)
		return make_shared<VoidType>();
	return (_operator == Token::NOT) ? shared_from_this() : TypePointer();
}

TypePointer BoolType::binaryOperatorResult(Token::Value _operator, TypePointer const& _other) const
{
	if (getCategory() != _other->getCategory())
		return TypePointer();
	if (Token::isCompareOp(_operator) || _operator == Token::AND || _operator == Token::OR)
		return _other;
	else
		return TypePointer();
}

bool ContractType::isImplicitlyConvertibleTo(Type const& _convertTo) const
{
	if (*this == _convertTo)
		return true;
	if (_convertTo.getCategory() == Category::INTEGER)
		return dynamic_cast<IntegerType const&>(_convertTo).isAddress();
	if (_convertTo.getCategory() == Category::CONTRACT)
	{
		auto const& bases = getContractDefinition().getLinearizedBaseContracts();
		if (m_super && bases.size() <= 1)
			return false;
		return find(m_super ? ++bases.begin() : bases.begin(), bases.end(),
					&dynamic_cast<ContractType const&>(_convertTo).getContractDefinition()) != bases.end();
	}
	return false;
}

bool ContractType::isExplicitlyConvertibleTo(Type const& _convertTo) const
{
	return isImplicitlyConvertibleTo(_convertTo) || _convertTo.getCategory() == Category::INTEGER ||
			_convertTo.getCategory() == Category::CONTRACT;
}

TypePointer ContractType::unaryOperatorResult(Token::Value _operator) const
{
	return _operator == Token::DELETE ? make_shared<VoidType>() : TypePointer();
}

bool ContractType::operator==(Type const& _other) const
{
	if (_other.getCategory() != getCategory())
		return false;
	ContractType const& other = dynamic_cast<ContractType const&>(_other);
	return other.m_contract == m_contract && other.m_super == m_super;
}

string ContractType::toString() const
{
	return "contract " + string(m_super ? "super " : "") + m_contract.getName();
}

MemberList const& ContractType::getMembers() const
{
	// We need to lazy-initialize it because of recursive references.
	if (!m_members)
	{
		// All address members and all interface functions
		map<string, shared_ptr<Type const>> members(IntegerType::AddressMemberList.begin(),
													IntegerType::AddressMemberList.end());
		if (m_super)
		{
			for (ContractDefinition const* base: m_contract.getLinearizedBaseContracts())
				for (ASTPointer<FunctionDefinition> const& function: base->getDefinedFunctions())
					if (!function->isConstructor() && !function->getName().empty())
						members.insert(make_pair(function->getName(), make_shared<FunctionType>(*function, true)));
		}
		else
			for (auto const& it: m_contract.getInterfaceFunctions())
				members[it.second->getDeclaration().getName()] = it.second;
		m_members.reset(new MemberList(members));
	}
	return *m_members;
}

shared_ptr<FunctionType const> const& ContractType::getConstructorType() const
{
	if (!m_constructorType)
	{
		FunctionDefinition const* constructor = m_contract.getConstructor();
		if (constructor)
			m_constructorType = make_shared<FunctionType>(*constructor);
		else
			m_constructorType = make_shared<FunctionType>(TypePointers(), TypePointers());
	}
	return m_constructorType;
}

u256 ContractType::getFunctionIdentifier(string const& _functionName) const
{
	auto interfaceFunctions = m_contract.getInterfaceFunctions();
	for (auto const& it: m_contract.getInterfaceFunctions())
		if (it.second->getDeclaration().getName() == _functionName)
			return FixedHash<4>::Arith(it.first);

	return Invalid256;
}

TypePointer StructType::unaryOperatorResult(Token::Value _operator) const
{
	return _operator == Token::DELETE ? make_shared<VoidType>() : TypePointer();
}

bool StructType::operator==(Type const& _other) const
{
	if (_other.getCategory() != getCategory())
		return false;
	StructType const& other = dynamic_cast<StructType const&>(_other);
	return other.m_struct == m_struct;
}

u256 StructType::getStorageSize() const
{
	u256 size = 0;
	for (pair<string, shared_ptr<Type const>> const& member: getMembers())
		size += member.second->getStorageSize();
	return max<u256>(1, size);
}

bool StructType::canLiveOutsideStorage() const
{
	for (pair<string, shared_ptr<Type const>> const& member: getMembers())
		if (!member.second->canLiveOutsideStorage())
			return false;
	return true;
}

string StructType::toString() const
{
	return string("struct ") + m_struct.getName();
}

MemberList const& StructType::getMembers() const
{
	// We need to lazy-initialize it because of recursive references.
	if (!m_members)
	{
		map<string, shared_ptr<Type const>> members;
		for (ASTPointer<VariableDeclaration> const& variable: m_struct.getMembers())
			members[variable->getName()] = variable->getType();
		m_members.reset(new MemberList(members));
	}
	return *m_members;
}

u256 StructType::getStorageOffsetOfMember(string const& _name) const
{
	//@todo cache member offset?
	u256 offset;
	for (ASTPointer<VariableDeclaration> variable: m_struct.getMembers())
	{
		if (variable->getName() == _name)
			return offset;
		offset += variable->getType()->getStorageSize();
	}
	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Storage offset of non-existing member requested."));
}

FunctionType::FunctionType(FunctionDefinition const& _function, bool _isInternal):
	m_location(_isInternal ? Location::INTERNAL : Location::EXTERNAL),
	m_isConstant(_function.isDeclaredConst()),
	m_declaration(&_function)
{
	TypePointers params;
	vector<string> paramNames;
	TypePointers retParams;
	vector<string> retParamNames;

	params.reserve(_function.getParameters().size());
	paramNames.reserve(_function.getParameters().size());
	for (ASTPointer<VariableDeclaration> const& var: _function.getParameters())
	{
		paramNames.push_back(var->getName());
		params.push_back(var->getType());
	}
	retParams.reserve(_function.getReturnParameters().size());
	retParamNames.reserve(_function.getReturnParameters().size());
	for (ASTPointer<VariableDeclaration> const& var: _function.getReturnParameters())
	{
		retParamNames.push_back(var->getName());
		retParams.push_back(var->getType());
	}
	swap(params, m_parameterTypes);
	swap(paramNames, m_parameterNames);
	swap(retParams, m_returnParameterTypes);
	swap(retParamNames, m_returnParameterNames);
}

FunctionType::FunctionType(VariableDeclaration const& _varDecl):
	m_location(Location::EXTERNAL), m_isConstant(true), m_declaration(&_varDecl)
{
	TypePointers params;
	vector<string> paramNames;
	TypePointers retParams;
	vector<string> retParamNames;
	TypePointer varDeclType = _varDecl.getType();
	auto mappingType = dynamic_cast<MappingType const*>(varDeclType.get());
	auto returnType = varDeclType;

	while (mappingType != nullptr)
	{
		params.push_back(mappingType->getKeyType());
		paramNames.push_back("");
		returnType = mappingType->getValueType();
		mappingType = dynamic_cast<MappingType const*>(mappingType->getValueType().get());
	}

	retParams.push_back(returnType);
	retParamNames.push_back("");

	swap(params, m_parameterTypes);
	swap(paramNames, m_parameterNames);
	swap(retParams, m_returnParameterTypes);
	swap(retParamNames, m_returnParameterNames);
}

FunctionType::FunctionType(const EventDefinition& _event):
	m_location(Location::EVENT), m_declaration(&_event)
{
	TypePointers params;
	vector<string> paramNames;
	params.reserve(_event.getParameters().size());
	paramNames.reserve(_event.getParameters().size());
	for (ASTPointer<VariableDeclaration> const& var: _event.getParameters())
	{
		paramNames.push_back(var->getName());
		params.push_back(var->getType());
	}
	swap(params, m_parameterTypes);
	swap(paramNames, m_parameterNames);
}

bool FunctionType::operator==(Type const& _other) const
{
	if (_other.getCategory() != getCategory())
		return false;
	FunctionType const& other = dynamic_cast<FunctionType const&>(_other);

	if (m_location != other.m_location)
		return false;
	if (m_isConstant != other.isConstant())
		return false;

	if (m_parameterTypes.size() != other.m_parameterTypes.size() ||
			m_returnParameterTypes.size() != other.m_returnParameterTypes.size())
		return false;
	auto typeCompare = [](TypePointer const& _a, TypePointer const& _b) -> bool { return *_a == *_b; };

	if (!equal(m_parameterTypes.cbegin(), m_parameterTypes.cend(),
			   other.m_parameterTypes.cbegin(), typeCompare))
		return false;
	if (!equal(m_returnParameterTypes.cbegin(), m_returnParameterTypes.cend(),
			   other.m_returnParameterTypes.cbegin(), typeCompare))
		return false;
	//@todo this is ugly, but cannot be prevented right now
	if (m_gasSet != other.m_gasSet || m_valueSet != other.m_valueSet)
		return false;
	return true;
}

string FunctionType::toString() const
{
	string name = "function (";
	for (auto it = m_parameterTypes.begin(); it != m_parameterTypes.end(); ++it)
		name += (*it)->toString() + (it + 1 == m_parameterTypes.end() ? "" : ",");
	name += ") returns (";
	for (auto it = m_returnParameterTypes.begin(); it != m_returnParameterTypes.end(); ++it)
		name += (*it)->toString() + (it + 1 == m_returnParameterTypes.end() ? "" : ",");
	return name + ")";
}

unsigned FunctionType::getSizeOnStack() const
{
	unsigned size = 0;
	if (m_location == Location::EXTERNAL)
		size = 2;
	else if (m_location == Location::INTERNAL || m_location == Location::BARE)
		size = 1;
	if (m_gasSet)
		size++;
	if (m_valueSet)
		size++;
	return size;
}

MemberList const& FunctionType::getMembers() const
{
	switch (m_location)
	{
	case Location::EXTERNAL:
	case Location::CREATION:
	case Location::ECRECOVER:
	case Location::SHA256:
	case Location::RIPEMD160:
	case Location::BARE:
		if (!m_members)
		{
			map<string, TypePointer> members{
				{"gas", make_shared<FunctionType>(parseElementaryTypeVector({"uint"}),
												  TypePointers{copyAndSetGasOrValue(true, false)},
												  Location::SET_GAS, m_gasSet, m_valueSet)},
				{"value", make_shared<FunctionType>(parseElementaryTypeVector({"uint"}),
													TypePointers{copyAndSetGasOrValue(false, true)},
													Location::SET_VALUE, m_gasSet, m_valueSet)}};
			if (m_location == Location::CREATION)
				members.erase("gas");
			m_members.reset(new MemberList(members));
		}
		return *m_members;
	default:
		return EmptyMemberList;
	}
}

string FunctionType::getCanonicalSignature(std::string const& _name) const
{
	std::string funcName = _name;
	if (_name == "")
	{
		solAssert(m_declaration != nullptr, "Function type without name needs a declaration");
		funcName = m_declaration->getName();
	}
	string ret = funcName + "(";

	for (auto it = m_parameterTypes.cbegin(); it != m_parameterTypes.cend(); ++it)
		ret += (*it)->toString() + (it + 1 == m_parameterTypes.cend() ? "" : ",");

	return ret + ")";
}

TypePointers FunctionType::parseElementaryTypeVector(strings const& _types)
{
	TypePointers pointers;
	pointers.reserve(_types.size());
	for (string const& type: _types)
		pointers.push_back(Type::fromElementaryTypeName(Token::fromIdentifierOrKeyword(type)));
	return pointers;
}

TypePointer FunctionType::copyAndSetGasOrValue(bool _setGas, bool _setValue) const
{
	return make_shared<FunctionType>(m_parameterTypes, m_returnParameterTypes, m_location,
									 m_gasSet || _setGas, m_valueSet || _setValue);
}

vector<string> const FunctionType::getParameterTypeNames() const
{
	vector<string> names;
	for (TypePointer const& t: m_parameterTypes)
		names.push_back(t->toString());

	return names;
}

vector<string> const FunctionType::getReturnParameterTypeNames() const
{
	vector<string> names;
	for (TypePointer const& t: m_returnParameterTypes)
		names.push_back(t->toString());

	return names;
}

ASTPointer<ASTString> FunctionType::getDocumentation() const
{
	auto function = dynamic_cast<Documented const*>(m_declaration);
	if (function)
		return function->getDocumentation();

	return ASTPointer<ASTString>();
}

bool MappingType::operator==(Type const& _other) const
{
	if (_other.getCategory() != getCategory())
		return false;
	MappingType const& other = dynamic_cast<MappingType const&>(_other);
	return *other.m_keyType == *m_keyType && *other.m_valueType == *m_valueType;
}

string MappingType::toString() const
{
	return "mapping(" + getKeyType()->toString() + " => " + getValueType()->toString() + ")";
}

bool TypeType::operator==(Type const& _other) const
{
	if (_other.getCategory() != getCategory())
		return false;
	TypeType const& other = dynamic_cast<TypeType const&>(_other);
	return *getActualType() == *other.getActualType();
}

MemberList const& TypeType::getMembers() const
{
	// We need to lazy-initialize it because of recursive references.
	if (!m_members)
	{
		map<string, TypePointer> members;
		if (m_actualType->getCategory() == Category::CONTRACT && m_currentContract != nullptr)
		{
			ContractDefinition const& contract = dynamic_cast<ContractType const&>(*m_actualType).getContractDefinition();
			vector<ContractDefinition const*> currentBases = m_currentContract->getLinearizedBaseContracts();
			if (find(currentBases.begin(), currentBases.end(), &contract) != currentBases.end())
				// We are accessing the type of a base contract, so add all public and private
				// functions. Note that this does not add inherited functions on purpose.
				for (ASTPointer<FunctionDefinition> const& f: contract.getDefinedFunctions())
					if (!f->isConstructor() && !f->getName().empty())
						members[f->getName()] = make_shared<FunctionType>(*f);
		}
		m_members.reset(new MemberList(members));
	}
	return *m_members;
}

ModifierType::ModifierType(const ModifierDefinition& _modifier)
{
	TypePointers params;
	params.reserve(_modifier.getParameters().size());
	for (ASTPointer<VariableDeclaration> const& var: _modifier.getParameters())
		params.push_back(var->getType());
	swap(params, m_parameterTypes);
}

bool ModifierType::operator==(Type const& _other) const
{
	if (_other.getCategory() != getCategory())
		return false;
	ModifierType const& other = dynamic_cast<ModifierType const&>(_other);

	if (m_parameterTypes.size() != other.m_parameterTypes.size())
		return false;
	auto typeCompare = [](TypePointer const& _a, TypePointer const& _b) -> bool { return *_a == *_b; };

	if (!equal(m_parameterTypes.cbegin(), m_parameterTypes.cend(),
			   other.m_parameterTypes.cbegin(), typeCompare))
		return false;
	return true;
}

string ModifierType::toString() const
{
	string name = "modifier (";
	for (auto it = m_parameterTypes.begin(); it != m_parameterTypes.end(); ++it)
		name += (*it)->toString() + (it + 1 == m_parameterTypes.end() ? "" : ",");
	return name + ")";
}

MagicType::MagicType(MagicType::Kind _kind):
	m_kind(_kind)
{
	switch (m_kind)
	{
	case Kind::BLOCK:
		m_members = MemberList({{"coinbase", make_shared<IntegerType>(0, IntegerType::Modifier::ADDRESS)},
								{"timestamp", make_shared<IntegerType>(256)},
								{"blockhash", make_shared<FunctionType>(strings{"uint"}, strings{"hash"}, FunctionType::Location::BLOCKHASH)},
								{"difficulty", make_shared<IntegerType>(256)},
								{"number", make_shared<IntegerType>(256)},
								{"gaslimit", make_shared<IntegerType>(256)}});
		break;
	case Kind::MSG:
		m_members = MemberList({{"sender", make_shared<IntegerType>(0, IntegerType::Modifier::ADDRESS)},
								{"gas", make_shared<IntegerType>(256)},
								{"value", make_shared<IntegerType>(256)}});
		break;
	case Kind::TX:
		m_members = MemberList({{"origin", make_shared<IntegerType>(0, IntegerType::Modifier::ADDRESS)},
								{"gasprice", make_shared<IntegerType>(256)}});
		break;
	default:
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unknown kind of magic."));
	}
}

bool MagicType::operator==(Type const& _other) const
{
	if (_other.getCategory() != getCategory())
		return false;
	MagicType const& other = dynamic_cast<MagicType const&>(_other);
	return other.m_kind == m_kind;
}

string MagicType::toString() const
{
	switch (m_kind)
	{
	case Kind::BLOCK:
		return "block";
	case Kind::MSG:
		return "msg";
	case Kind::TX:
		return "tx";
	default:
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unknown kind of magic."));
	}
}

}
}
