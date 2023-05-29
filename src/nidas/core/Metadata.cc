/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2023, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include <nidas/util/Logger.h>

#include <iostream>
#include <sstream>
#include <iomanip>

#include "Metadata.h"

#include <json/value.h>
#include <json/writer.h>
#include <json/reader.h>

namespace nidas {
namespace core {

using nidas::util::UTime;


struct MetadataImpl: public Json::Value
{};


MetadataException::
MetadataException(const std::string& what):
    std::runtime_error(what)
{}

MetadataException::
MetadataException(const std::ostringstream& buf):
    std::runtime_error(buf.str())
{}


MetadataItem::
MetadataItem(MetadataInterface* mdi, enum MODE mode,
             const std::string& name,
             const std::string& description):
    _mdi(mdi),
    _mode(mode),
    _name(name),
    _description(description)
{
    mdi->add_item(this);
}

MetadataInterface*
MetadataItem::mdi()
{
    return _mdi;
}

/**
 * Also convenient to give subclasses access directly to the dictionary.
 */
MetadataImpl&
MetadataItem::mdict()
{
    return _mdi->metadata().mdict();
}


bool
MetadataItem::
unset() const
{
    MetadataImpl& jd = const_cast<MetadataItem*>(this)->mdict();
    // an item is unset if it is not a member or set to null.
    return jd.get(_name, Json::Value()).isNull();
}

void
MetadataItem::
erase()
{
    // this is a little ambiguous in that the key could either be set to null
    // or removed to erase it, but this explicitly removes it.
    MetadataImpl& jd = mdict();
    jd.removeMember(_name);
}

MetadataItem&
MetadataItem::
operator=(const MetadataItem& right)
{
    // just copy the underlying json value if set.  we don't care if the
    // assignment makes sense, validation is separate.
    if (!right.unset())
    {
        MetadataImpl& jd = mdict();
        MetadataImpl& jd2 = const_cast<MetadataItem*>(&right)->mdict();
        jd[_name] = jd2[right._name];
    }
    return *this;
}


#ifdef notdef
MetadataItem::
MetadataItem(const MetadataItem&)
{
    // The default initializers for the MetadataItem members should take
    // precedence, so we don't want to change any of that state, especially
    // not the pointer to the containing interface.  And since any metadata
    // values are copied by containing interface, there's no point to copy
    // that here either.

    // According to the standard, default member initializers are used if a
    // member is not in the initialization list.  Unfortunately, that can
    // cause warnings about "NNN should be initialized in the member
    // initialization list".
}
#endif

bool
MetadataItem::
check_assign_string(const std::string& incoming)
{
    update_string_value(incoming);
    return true;
}


void
MetadataItem::
update_string_value(const std::string& value)
{
    MetadataImpl& jd = mdict();
    jd[_name] = Json::Value(value);
    // someday this could be a more elaborate kind of notification.
    DLOG(("") << "updated metadata: " << _name << "=" << value);
}



std::ostream&
operator<<(std::ostream& out, const MetadataItem& item)
{
    return (out << item.string_value());
}


std::string
MetadataItem::
string_value() const
{
    Metadata& md = const_cast<MetadataItem*>(this)->mdi()->metadata();
    return md.string_value(name());
}



MetadataItem::
~MetadataItem()
{

}


// MetadataString implementation

MetadataString::
MetadataString(MetadataInterface* mdi, enum MODE mode,
                const std::string& name,
                const std::string& description):
    MetadataItem(mdi, mode, name, description)
{}

std::string
MetadataString::
get()
{
    if (unset())
        return "";
    return string_value();
}


MetadataString::
operator std::string()
{
    return get();
}

/**
 * Set the value of this item.  Return true if the setting succeeded,
 * meaning the value was valid, and otherwise false.
 */
bool
MetadataString::
set(const std::string& value)
{
    return check_assign_string(value);
}

/**
 * Allow direct string assignment.  If the assignment fails because of
 * constraint checks, then error() will be non-empty.
 */
MetadataString&
MetadataString::
operator=(const std::string& value)
{
    this->set(value);
    return *this;
}

// MetadataBool implementation

MetadataBool::
MetadataBool(MetadataInterface* mdi, enum MODE mode,
                const std::string& name,
                const std::string& description):
    MetadataItem(mdi, mode, name, description)
{}

bool
MetadataBool::
check_assign_string(const std::string& incoming)
{
    bool target{false};
    if (incoming == "true")
        target = true;
    else if (incoming == "false")
        target = false;
    else
    {
        mdi()->metadata().add_error("could not parse as bool: " + incoming);
        return false;
    }
    MetadataImpl& jd = mdict();
    jd[name()] = Json::Value(target);
    return true;
}

bool
MetadataBool::
get()
{
    MetadataImpl& jd = mdict();
    return jd.get(name(), Json::Value(false)).asBool();
}

MetadataBool::
operator bool()
{
    return get();
}

#ifdef notdef
bool
MetadataBool::
from_string(const std::string& incoming)
{
    bool target{false};
    if (incoming == "true")
        target = true;
    else if (incoming == "false")
        target = false;
    else
    {
        mdi()->metadata().add_error("could not parse as bool: " + incoming);
    }
    return target;
}

std::string
MetadataBool::
to_string(bool value)
{
    return value ? "true" : "false";
}
#endif

bool
MetadataBool::
set(bool value)
{
    MetadataImpl& jd = mdict();
    jd[name()] = Json::Value(value);
    return true;
}


// MetadataNumber<T>

template <typename T>
MetadataNumber<T>::
MetadataNumber(MetadataInterface* mdi, enum MODE mode,
               const std::string& name,
               const std::string& description,
               int precision_, T min_, T max_):
    MetadataItem(mdi, mode, name, description),
    precision(precision_), min(min_), max(max_)
{}

template <typename T>
T
MetadataNumber<T>::
get() const
{
    if (unset())
        return T();
    MetadataImpl& jd = const_cast<MetadataNumber<T>*>(this)->mdict();
    return jd[name()].asDouble();
}

template <typename T>
MetadataNumber<T>::
operator T()
{
    return get();
}

template <typename T>
bool
MetadataNumber<T>::
set(const T& value)
{
    if (value < min || max < value)
    {
        Metadata& md = mdi()->metadata();
        md.add_error(errbuf() << value << " is not in range ["
                           << min << ", " << max << "]");
        return false;
    }
    MetadataImpl& jd = mdict();
    jd[name()] = value;
    return true;
}

template <typename T>
MetadataNumber<T>&
MetadataNumber<T>::
operator=(const std::string& value)
{
    check_assign_string(value);
    return *this;
}

template <typename T>
MetadataNumber<T>&
MetadataNumber<T>::
operator=(const T& value)
{
    this->set(value);
    return *this;
}

template <typename T>
bool
MetadataNumber<T>::
check_assign_string(const std::string& incoming)
{
    Metadata& md = mdi()->metadata();
    T target = from_string(incoming);
    if (!md.errors().empty())
    {
        return false;
    }
    if (target < min || max < target)
    {
        md.add_error(errbuf() << target << " is not in range ["
                           << min << ", " << max << "]");
        return false;
    }
    update_string_value(to_string(target));
    return true;
}

template <typename T>
std::string
MetadataNumber<T>::
to_string(const T& value) const
{
    std::ostringstream outb;
    outb << std::setprecision(precision) << value;
    return outb.str();
}

template <typename T>
std::string
MetadataNumber<T>::
string_value() const
{
    if (unset())
        return "";
    return to_string(get());
}


template <typename T>
T
MetadataNumber<T>::
from_string(const std::string& value)
{
    Metadata& md = mdi()->metadata();
    std::istringstream inb(value);
    T target{0};
    if (!(inb >> target))
    {
        md.add_error(errbuf("could not parse as a number: ") << value);
    }
    return target;
}

template <typename T>
MetadataNumber<T>&
MetadataNumber<T>::
operator=(const MetadataNumber<T>& right)
{
    MetadataItem::operator=(right);
    return *this;
}

// explicit instantiation of the numeric types with aliases.
template class MetadataNumber<double>;
template class MetadataNumber<int>;


// MetadataTime implementation

MetadataTime::
MetadataTime(MetadataInterface* mdi, enum MODE mode,
                const std::string& name,
                const std::string& description):
    MetadataItem(mdi, mode, name, description)
{
}

UTime
MetadataTime::
get()
{
    // no point trying to parse an unset value.
    UTime ut(0l);
    if (!unset())
        from_string(ut, string_value());
    return ut;
}

bool
MetadataTime::
set(const UTime& value)
{
    // no need to convert to a string just to call check_assign_string() and
    // convert it back, because we know we get a valid string from to_iso().
    update_string_value(value.to_iso());
    return true;
}

MetadataTime&
MetadataTime::
operator=(const UTime& ut)
{
    set(ut);
    return *this;
}

MetadataTime&
MetadataTime::
operator=(const std::string& value)
{
    check_assign_string(value);
    return *this;
}

bool
MetadataTime::
check_assign_string(const std::string& incoming)
{
    UTime ut(0l);
    if (!from_string(ut, incoming))
        return false;
    update_string_value(ut.to_iso());
    return true;
}

bool
MetadataTime::
from_string(UTime& ut, const std::string& value)
{
    Metadata& md = mdi()->metadata();
    if (!ut.from_iso(value))
    {
        md.add_error("could not parse time: " + value);
        return false;
    }
    return true;
};


// Metadata implementation

Metadata::
Metadata():
    _errors(),
    _interfaces(),
    _dict(new MetadataImpl)
{
    DLOG(("") << "Metadata constructor");
}


Metadata::
~Metadata()
{
    DLOG(("") << "Metadata destructor");
}



Metadata::
Metadata(const Metadata& right):
    _errors(right._errors),
    _interfaces(),
    _dict(new MetadataImpl(*right._dict.get()))
{
    // The interfaces are not copied here, since they need to be bound to this
    // Metadata instance through it's shared pointer.  That happens in the
    // MetadataInterface assignment method.
}


Metadata&
Metadata::
operator=(const Metadata& right)
{
    if (this == &right)
        return *this;
    *_dict = *right._dict;
    _errors = right._errors;
    for (auto& iface: right._interfaces)
    {
        if (!get_interface(iface->classname()))
            _interfaces.emplace_back(iface->clone());
    }
    return *this;
}


MetadataInterface*
Metadata::
add_interface(MetadataInterface* mdi)
{
    _interfaces.emplace_back(mdi);
    return mdi;
}


MetadataInterface*
Metadata::
get_interface(const std::string& name)
{
    for (auto& iface: _interfaces)
    {
        if (iface->classname() == name)
            return iface.get();
    }
    return nullptr;
}


Metadata::interface_list
Metadata::
interfaces()
{
    interface_list interfaces;
    for (auto& iface: _interfaces)
    {
        interfaces.push_back(iface.get());
    }
    return interfaces;
}




#ifdef notdef
SensorMetadata::item_list
SensorMetadata::
get_items()
{
    item_list items{
        &record_type,
        &timestamp,
        &manufacturer,
        &model,
        &serial_number,
        &hardware_version,
        &manufacture_date,
        &firmware_version,
        &firmware_build,
        &calibration_date
    };
    enumerate(items);
    // any items added at runtime and stored by the base class would be
    // appended here.
    return items;
}
#endif


std::string
Metadata::
string_value(const std::string& name)
{
    MetadataImpl& jd = mdict();
    Json::Value value = jd.get(name, Json::Value(std::string()));
    if (value.isString())
    {
        return value.asString();
    }
    std::ostringstream buf;
    buf << value;
    return buf.str();
}



std::string
Metadata::
to_buffer(int nindent)
{
    Json::StreamWriterBuilder wbuilder;
    wbuilder[std::string("indentation")] = std::string(nindent, ' ');
    // since this is the default precision for number items.
    wbuilder[std::string("precision")] = 12;
    return Json::writeString(wbuilder, *_dict.get());
}


bool
Metadata::
from_buffer(const std::string& buffer)
{
    Json::CharReaderBuilder rbuilder;
    std::string errs;
    std::istringstream inbuf(buffer);
    bool ok = Json::parseFromStream(rbuilder, inbuf, _dict.get(), &errs);
    if (!ok)
    {
        add_error(errs);
        PLOG(("json parse failed: ") << buffer);
    }
    return ok;
}


#ifdef notdef
std::string
Metadata::
to_buffer(int nindent)
{
    
    std::ostringstream buf;
    std::string indent(nindent, ' ');
    std::string separator{' '};
    std::string comma;
    if (nindent)
        separator = '\n';
    buf << "{";
    for (auto mdi: get_items())
    {
        if (mdi->unset())
            continue;
        buf << comma << separator << indent << "\"" << mdi->name() << "\": ";
        if (auto si = dynamic_cast<MetadataString*>(mdi))
        {
            // have to surround strings with quotes and escape embedded
            // quotes, so let the json library take care of that.
            json::value v;
            v.emplace_string() = si->get();
            buf << v;
        }
        else if (auto ti = dynamic_cast<MetadataTime*>(mdi))
        {
            json::value v;
            v.emplace_string() = ti->string_value();
            buf << v;
        }
        else
        {
            // the string value is good enough
            buf << mdi->string_value();
        }
        comma = ",";
    }
    buf << separator << "}";
    return buf.str();
}
#endif

#ifdef notdef
void
MetadataBool::
visit(MetadataItemVisitor* visitor)
{
    visitor->visit_bool(this);
}

template <>
void
MetadataNumber<double>::
visit(MetadataItemVisitor* visitor)
{
    visitor->visit_double(this);
}

template <>
void
MetadataNumber<int>::
visit(MetadataItemVisitor* visitor)
{
    visitor->visit_int(this);
}

void
MetadataString::
visit(MetadataItemVisitor* visitor)
{
    visitor->visit_string(this);
}

void
MetadataTime::
visit(MetadataItemVisitor* visitor)
{
    visitor->visit_time(this);
}


void MetadataItemVisitor::visit_double(MetadataDouble*) {}
void MetadataItemVisitor::visit_int(MetadataInt*) {}
void MetadataItemVisitor::visit_string(MetadataString*) {}
void MetadataItemVisitor::visit_bool(MetadataBool*) {}
void MetadataItemVisitor::visit_time(MetadataTime*) {}
MetadataItemVisitor::~MetadataItemVisitor() {}


class FillValue: public MetadataItemVisitor
{
public:
    json::value v{};

    virtual void visit_double(MetadataDouble* di)
    {
        v.emplace_double() = di->get();
    }

    virtual void visit_int(MetadataInt* ii)
    {
        v.emplace_int64() = ii->get();
    }

    virtual void visit_string(MetadataString* si)
    {
        v.emplace_string() = si->get();
    }

    virtual void visit_bool(MetadataBool* bi)
    {
        v.emplace_bool() = bi->get();
    }
    virtual void visit_time(MetadataTime* ti)
    {
        v.emplace_string() = ti->string_value();
    }

    virtual ~FillValue() {}
};


/**
 * Rather than check the underlying value type in every implementation, expect
 * the caller to catch an exception if any of the accesses do not match.
 */
class LoadValue: public MetadataItemVisitor
{
public:
    json::value v{};

    virtual void visit_double(MetadataDouble* di)
    {
        // json::value does not cast for us
        if (v.is_int64())
            di->set(v.as_int64());
        else if (v.is_uint64())
            di->set(v.as_uint64());
        else
            di->set(v.as_double());
    }

    virtual void visit_int(MetadataInt* ii)
    {
        if (v.is_uint64())
            ii->set(v.as_uint64());
        else
            ii->set(v.as_int64());
    }

    virtual void visit_string(MetadataString* si)
    {
        si->set(v.as_string().c_str());
    }

    virtual void visit_bool(MetadataBool* bi)
    {
        bi->set(v.as_bool());
    }

    virtual void visit_time(MetadataTime* ti)
    {
        ti->check_assign_string(v.as_string().c_str());
    }

    virtual ~LoadValue() {}
};


bool
Metadata::
from_buffer(const std::string& buffer)
{
    bool ok = true;
    json::error_code ec;
    json::value obj = json::parse(buffer, ec);
    if (ec)
    {
        PLOG(("json parse failed: ") << buffer);
        return false;
    }
    try {
        for (auto& mp: obj.as_object())
        {
            MetadataItem* mdi = lookup(mp.key_c_str());
            if (!mdi)
            {
                throw MetadataException(
                    std::ostringstream("unknown metadata item: ") << mp.key());
            }
            else
            {
                LoadValue lv;
                lv.v = mp.value();
                mdi->visit(&lv);
            }
        }
    }
    catch (std::exception& ex)
    {
        PLOG(("") << ex.what());
        ok = false;
    }
    return ok;
}
#endif


SensorMetadata::
SensorMetadata(const std::string& classname):
    MetadataInterface(classname)
{}


#ifdef notdef
/**
 * This version relies solely on boost::json for formatting, and that does not
 * offer an indentation setting.  The floats are at least written to the
 * minimal precision, but with an exponent suffix to differentiate floats from
 * ints.
 */
std::string
Metadata::
to_buffer(int nindent)
{
    json::object obj;
    for (auto mdi: get_items())
    {
        if (mdi->unset())
            continue;
        FillValue fv;
        mdi->visit(&fv);
        obj[mdi->name()] = fv.v;
    }
    std::ostringstream buf;
    buf << json::serialize(obj);
    return buf.str();
}
#endif


#ifdef notdef
/**
 * This implementation uses visitor instead of dynamic_cast to get at the item
 * type, but still performs formatting manually. boost::json explicilty writes
 * floats with exponents, so this version includes that.
 */
std::string
Metadata::
to_buffer(int nindent)
{
    std::ostringstream buf;
    std::string indent(nindent, ' ');
    std::string separator{' '};
    std::string comma;
    if (nindent)
        separator = '\n';
    buf << "{";
    for (auto mdi: get_items())
    {
        if (mdi->unset())
            continue;
        buf << comma << separator << indent << "\"" << mdi->name() << "\": ";
        FillValue fv;
        mdi->visit(&fv);
        buf << fv.v;
        comma = ",";
    }
    buf << separator << "}";
    return buf.str();
}
#endif


MetadataInterface::
MetadataInterface(const std::string& classname):
    _md(nullptr),
    _owned_md(),
    _classname(classname),
    _items()
{
    DLOG(("") << _classname << " constructor");
}


MetadataInterface::
~MetadataInterface()
{
    static nidas::util::LogContext lp(LOG_DEBUG);
    if (lp.active())
    {
        lp.log()
         << _classname << " destructor: "
         << "owned metadata ptr is " << _owned_md.get();
    }
}



#ifdef notdef
MetadataInterface::
MetadataInterface(const MetadataInterface& right):
    _md(),
    _classname(right._classname),
    _items()
{
    // make sure to call metadata() to create our own metadata instance,
    // into which the right metadata can be copied.  this will copy the
    // interfaces on the right metadata also, unless this one already has
    // it.  note that no subclass items have been added yet, but that's ok
    // since their initialization doesn't matter here.
    *this = right;
}
#endif


MetadataInterface&
MetadataInterface::
operator=(const MetadataInterface& right)
{
    Metadata& md = const_cast<MetadataInterface*>(&right)->metadata();
    metadata() = md;

    // now copy any additional interfaces from the right Metadata and bind
    // them to the Metadata in this instance.
    for (auto& iface: md.interfaces())
    {
        add_interface(*iface);
    }

    // because the interface being copied may have been the actual
    // instance type and not attached to the metadata yet, make a point of
    // attaching it to this copy.
    add_interface(right);
    return *this;
}


std::string
MetadataInterface::
string_value(const std::string& name)
{
    return metadata().string_value(name);
}

MetadataItem*
MetadataInterface::
lookup(const std::string& name)
{
    VLOG(("") << "lookup(" << name << ") in interface " << classname() << " with " << get_items().size() << " items.");
    for (auto mi: get_items())
    {
        VLOG(("...checking ") << mi->name());
        if (mi->name() == name)
            return mi;
    }
    return nullptr;
}


MetadataInterface::item_list
MetadataInterface::
get_items()
{
    return _items;
}

MetadataItem*
MetadataInterface::
add_item(MetadataItem* item)
{
    VLOG(("adding item '") << item->name() << "' to interface '" << classname() << "'");
    _items.push_back(item);
    return item;
}


void
MetadataInterface::
bind(Metadata* md)
{
    DLOG(("") << classname() << " binding new metadata");
    _md = md;
    // may as well release anything that was owned
    _owned_md.release();
}


Metadata&
MetadataInterface::
metadata()
{
    // The only way this object does not already have a metadata reference is
    // if it was created on its own and not by attaching it to the metadata of
    // an existing interface.  Typically this will be a local value instance.
    // So if this interface creates the metadata, then attach a clone of this
    // interface also.  That way it will be carried along with any copies of
    // this metadata.  Technically callers might have access to two of the
    // same interfaces on this metadata, but that is inconsequential since
    // both interfaces will modify the same metadata.
    //
    // actually we can't do that here, because if this is being called in the
    // base class constructor, then the virtual clone() will not dispatch to
    // the subclass yet.  so defer this until this interface is copied, and
    // then make a point of including a clone of this interface in the target.
    if (!_md)
    {
        DLOG(("") << classname() << ": creating metadata");
        _md = new Metadata();
        _owned_md.reset(_md);
        // add_interface(*this);
    }
    return *_md;
}


bool
MetadataInterface::
validate()
{
    return true;
}


} // namespace core
} // namespace nidas
