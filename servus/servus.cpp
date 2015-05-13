
/* Copyright (c) 2012-2015, Stefan Eilemann <eile@eyescale.ch>
 *
 * This file is part of Servus <https://github.com/Eyescale/Servus>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "servus.h"

#include <map>

// for NI_MAXHOST
#ifdef _WIN32
#  include <ws2tcpip.h>
#else
#  include <netdb.h>
#  include <unistd.h>
#endif

#include <string.h>

namespace servus
{
#define ANNOUNCE_TIMEOUT 1000 /*ms*/

namespace detail
{
static const std::string empty_;
typedef std::map< std::string, std::string > ValueMap;
typedef std::map< std::string, ValueMap > InstanceMap;
typedef ValueMap::const_iterator ValueMapCIter;
typedef InstanceMap::const_iterator InstanceMapCIter;

class Servus
{
public:
    Servus( const std::string& name ) : _name( name ) {}
    virtual ~Servus() {}

    virtual std::string getClassName() const = 0;

    const std::string& getName() const { return _name; }

    void set( const std::string& key, const std::string& value )
    {
        _data[ key ] = value;
        _updateRecord();
    }

    Strings getKeys() const
    {
        Strings keys;
        for( ValueMapCIter i = _data.begin(); i != _data.end(); ++i )
            keys.push_back( i->first );
        return keys;
    }

    const std::string& get( const std::string& key ) const
    {
        ValueMapCIter i = _data.find( key );
        if( i != _data.end( ))
            return i->second;
        return empty_;
    }

    virtual servus::Servus::Result announce( const unsigned short port,
                                               const std::string& instance ) =0;
    virtual void withdraw() = 0;
    virtual bool isAnnounced() const = 0;

    virtual servus::Servus::Result beginBrowsing(
        const servus::Servus::Interface interface_ ) = 0;
    virtual servus::Servus::Result browse( const int32_t timeout ) = 0;

    virtual void endBrowsing() = 0;
    virtual bool isBrowsing() const = 0;
    virtual Strings discover( const servus::Servus::Interface interface_,
                              const unsigned browseTime ) = 0;

    Strings getInstances() const
    {
        Strings instances;
        for( InstanceMapCIter i = _instanceMap.begin();
             i != _instanceMap.end(); ++i )
        {
            instances.push_back( i->first );
        }
        return instances;
    }

    Strings getKeys( const std::string& instance ) const
    {
        Strings keys;
        InstanceMapCIter i = _instanceMap.find( instance );
        if( i == _instanceMap.end( ))
            return keys;

        const ValueMap& values = i->second;
        for( ValueMapCIter j = values.begin(); j != values.end(); ++j )
            keys.push_back( j->first );
        return keys;
    }

    bool containsKey( const std::string& instance, const std::string& key )
        const
    {
        InstanceMapCIter i = _instanceMap.find( instance );
        if( i == _instanceMap.end( ))
            return false;

        const ValueMap& values = i->second;
        ValueMapCIter j = values.find( key );
        if( j == values.end( ))
            return false;
        return true;
    }

    const std::string& get( const std::string& instance,
                            const std::string& key ) const
    {
        InstanceMapCIter i = _instanceMap.find( instance );
        if( i == _instanceMap.end( ))
            return detail::empty_;

        const ValueMap& values = i->second;
        ValueMapCIter j = values.find( key );
        if( j == values.end( ))
            return detail::empty_;
        return j->second;
    }

    void getData( servus::Servus::Data& data ) const
    {
        data = _instanceMap;
    }

protected:
    const std::string _name;
    InstanceMap _instanceMap; //!< last discovered data
    ValueMap _data;   //!< self data to announce

    virtual void _updateRecord() = 0;
};

}
}

// Impls need detail interface definition above
#ifdef SERVUS_USE_DNSSD
#  include "dnssd/servus.h"
#elif defined(SERVUS_USE_AVAHI_CLIENT)
#  include "avahi/servus.h"
#endif
#include "none/servus.h"

namespace servus
{
bool Servus::isAvailable()
{
#if defined(SERVUS_USE_DNSSD) || defined(SERVUS_USE_AVAHI_CLIENT)
    return true;
#endif
    return false;
}

Servus::Servus( const std::string& name )
#ifdef SERVUS_USE_DNSSD
    : _impl( new dnssd::Servus( name ))
#elif defined(SERVUS_USE_AVAHI_CLIENT)
    : _impl( new avahi::Servus( name ))
#else
    : _impl( new none::Servus( name ))
#endif
{}

Servus::Servus( const Servus& from )
    : _impl( from._impl )
{}

Servus::~Servus()
{}

Servus& Servus::operator = ( const Servus& rhs )
{
    _impl = rhs._impl;
    return *this;
}

const std::string& Servus::getName() const
{
    return _impl->getName();
}

std::string Servus::Result::getString() const
{
    const int32_t code = getCode();
    switch( code )
    {
#ifdef SERVUS_USE_DNSSD
    case kDNSServiceErr_Unknown:           return "unknown error";
    case kDNSServiceErr_NoSuchName:        return "name not found";
    case kDNSServiceErr_NoMemory:          return "out of memory";
    case kDNSServiceErr_BadParam:          return "bad parameter";
    case kDNSServiceErr_BadReference:      return "bad reference";
    case kDNSServiceErr_BadState:          return "bad state";
    case kDNSServiceErr_BadFlags:          return "bad flags";
    case kDNSServiceErr_Unsupported:       return "unsupported";
    case kDNSServiceErr_NotInitialized:    return "not initialized";
    case kDNSServiceErr_AlreadyRegistered: return "already registered";
    case kDNSServiceErr_NameConflict:      return "name conflict";
    case kDNSServiceErr_Invalid:           return "invalid value";
    case kDNSServiceErr_Firewall:          return "firewall";
    case kDNSServiceErr_Incompatible:
        return "client library incompatible with daemon";
    case kDNSServiceErr_BadInterfaceIndex: return "bad interface index";
    case kDNSServiceErr_Refused:           return "refused";
    case kDNSServiceErr_NoSuchRecord:      return "no such record";
    case kDNSServiceErr_NoAuth:            return "no authentication";
    case kDNSServiceErr_NoSuchKey:         return "no such key";
    case kDNSServiceErr_NATTraversal:      return "NAT traversal";
    case kDNSServiceErr_DoubleNAT:         return "double NAT";
    case kDNSServiceErr_BadTime:           return "bad time";
#endif

    case PENDING:          return "operation pending";
    case NOT_SUPPORTED:    return "Servus compiled without ZeroConf support";
    case POLL_ERROR:       return "Error polling for events";
    default:
        if( code > 0 )
            return ::strerror( code );
        return servus::Result::getString();
    }
}

void Servus::set( const std::string& key, const std::string& value )
{
    _impl->set( key, value );
}

Strings Servus::getKeys() const
{
    return _impl->getKeys();
}

const std::string& Servus::get( const std::string& key ) const
{
    return _impl->get( key );
}

Servus::Result Servus::announce( const unsigned short port,
                                 const std::string& instance )
{
    return _impl->announce( port, instance );
}

void Servus::withdraw()
{
    _impl->withdraw();
}

bool Servus::isAnnounced() const
{
    return _impl->isAnnounced();
}

Strings Servus::discover( const Interface addr, const unsigned browseTime )
{
    return _impl->discover( addr, browseTime );
}

Servus::Result Servus::beginBrowsing( const servus::Servus::Interface addr )
{
    return _impl->beginBrowsing( addr );
}

Servus::Result Servus::browse( int32_t timeout )
{
    return _impl->browse( timeout );
}

void Servus::endBrowsing()
{
    _impl->endBrowsing();
}

bool Servus::isBrowsing() const
{
    return _impl->isBrowsing();
}

Strings Servus::getInstances() const
{
    return _impl->getInstances();
}

Strings Servus::getKeys( const std::string& instance ) const
{
    return _impl->getKeys( instance );
}

bool Servus::containsKey( const std::string& instance,
                          const std::string& key ) const
{
    return _impl->containsKey( instance, key );
}

const std::string& Servus::get( const std::string& instance,
                                const std::string& key ) const
{
    return _impl->get( instance, key );
}

void Servus::getData( Data& data )
{
    _impl->getData( data );
}

std::string getHostname()
{
    char hostname[NI_MAXHOST+1] = {0};
    gethostname( hostname, NI_MAXHOST );
    hostname[NI_MAXHOST] = '\0';
    return std::string( hostname );
}

std::ostream& operator << ( std::ostream& os, const Servus& servus )
{
    os << "Servus instance"
       << (servus.isAnnounced() ? " " : " not ") << "announced"
       << (servus.isBrowsing() ? " " : " not ") << "browsing, implementation"
       << servus._impl->getClassName();

    const Strings& keys = servus.getKeys();
    for( auto key : keys )
        os << std::endl << "    " << key << " = " << servus.get( key );

    return os;
}

std::ostream& operator << ( std::ostream& os , const Servus::Interface& addr )
{
    switch( addr )
    {
    case Servus::IF_ALL: return os << " all ";
    case Servus::IF_LOCAL: return os << " local ";
    }
    return os;
}

}
