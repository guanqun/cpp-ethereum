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
 * Scope - object that holds declaration of names.
 */

#include <libsolidity/DeclarationContainer.h>
#include <libsolidity/AST.h>
#include <libsolidity/Types.h>

namespace dev
{
namespace solidity
{

// _update is used as a checkDeclaration flag.
bool DeclarationContainer::registerDeclaration(Declaration const& _declaration, bool _update)
{
	if (_declaration.getName().empty())
		return true;

	TypePointer newType = _declaration.getType();
	if (newType->getCategory() == Type::Category::Function)
	{
		if (!_update)
		{
			auto declarations = resolveName(_declaration.getName(), true);
			if (!declarations.empty())
			{
				FunctionType const& newFunction = dynamic_cast<FunctionType const&>(*newType);
				std::string newFunctionSignature = newFunction.getCanonicalSignature();

				for (auto const* declaration : declarations)
				{
					auto type = declaration->getType();
					if (type->getCategory() != Type::Category::Function)
						return false;

					FunctionType const& function = dynamic_cast<FunctionType const&>(*type);
					if (function.getCanonicalSignature() == newFunctionSignature)
						return false;
				}
			}
		}
	}
	else
	{
		if (!_update && m_declarations.count(_declaration.getName()) != 0)
			return false;
	}

	auto declarations = resolveName(_declaration.getName(), true);
	bool needInsert = true;
	for (auto const* declaration : declarations)
		if (declaration == &_declaration)
		{
			needInsert = false;
			break;
		}
	if (needInsert)
		m_declarations.insert(make_pair(_declaration.getName(), &_declaration));
	return true;
}

std::vector<Declaration const*> DeclarationContainer::resolveName(ASTString const& _name, bool _recursive) const
{
	solAssert(!_name.empty(), "Attempt to resolve empty name.");
	std::vector<Declaration const*> declarations;
	auto range = m_declarations.equal_range(_name);
	for (auto it = range.first; it != range.second; ++it)
		declarations.push_back(it->second);
	if (!declarations.empty())
		return declarations;
	if (_recursive && m_enclosingContainer)
		return m_enclosingContainer->resolveName(_name, true);
	return declarations;
}

}
}
