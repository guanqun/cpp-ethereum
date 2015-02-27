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
/** @file NodeTable.cpp
 * @author Alex Leverington <nessence@gmail.com>
 * @date 2014
 */

#include "NodeTable.h"
using namespace std;
using namespace dev;
using namespace dev::p2p;

NodeTable::NodeTable(ba::io_service& _io, KeyPair _alias, uint16_t _listenPort):
	m_node(Node(_alias.pub(), bi::udp::endpoint())),
	m_secret(_alias.sec()),
	m_socket(new NodeSocket(_io, *this, _listenPort)),
	m_socketPtr(m_socket.get()),
	m_io(_io),
	m_bucketRefreshTimer(m_io),
	m_evictionCheckTimer(m_io)
{
}
	
NodeTable::~NodeTable()
{
}

void NodeTable::join()
{
}
	
list<NodeId> NodeTable::nodes() const
{
}

list<NodeTable::NodeEntry> NodeTable::state() const
{
}

NodeTable::NodeEntry NodeTable::operator[](NodeId _id)
{
	Guard l(x_nodes);
	return *m_nodes[_id];
}

void NodeTable::requestNeighbours(NodeEntry const& _node, NodeId _target) const
{
}

void NodeTable::doFindNode(NodeId _node, unsigned _round, shared_ptr<set<shared_ptr<NodeEntry>>> _tried)
{
}

vector<shared_ptr<NodeTable::NodeEntry>> NodeTable::findNearest(NodeId _target)
{
    vector<shared_ptr<NodeTable::NodeEntry>> temp;
    return temp;
}

void NodeTable::ping(bi::udp::endpoint _to) const
{
}

void NodeTable::ping(NodeEntry* _n) const
{
}

void NodeTable::evict(shared_ptr<NodeEntry> _leastSeen, shared_ptr<NodeEntry> _new)
{
}

void NodeTable::noteNode(Public const& _pubk, bi::udp::endpoint const& _endpoint)
{
}

void NodeTable::noteNode(shared_ptr<NodeEntry> _n)
{
}

void NodeTable::dropNode(shared_ptr<NodeEntry> _n)
{
}

NodeTable::NodeBucket& NodeTable::bucket(NodeEntry const* _n)
{
	return m_state[_n->distance - 1];
}

void NodeTable::onReceived(UDPSocketFace*, bi::udp::endpoint const& _from, bytesConstRef _packet)
{
}
	
void NodeTable::doCheckEvictions(boost::system::error_code const& _ec)
{
}

void NodeTable::doRefreshBuckets(boost::system::error_code const& _ec)
{
}

