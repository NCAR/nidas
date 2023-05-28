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


const std::string MetadataItem::UNSET{"UNSET"};
const std::string MetadataItem::FAILED{"FAILED"};


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
    if (unset())
        return UTime(0l);
    return from_string(string_value());
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
    Metadata& md = mdi()->metadata();
    UTime ut(from_string(incoming));
    if (!md.errors().empty())
        return false;
    update_string_value(ut.to_iso());
    return true;
}

UTime
MetadataTime::
from_string(const std::string& value)
{
    Metadata& md = mdi()->metadata();
    UTime ut(0l);
    if (!ut.from_iso(value))
    {
        md.add_error("could not parse time: " + value);
    }
    return ut;
};


// Metadata implementation

Metadata::
Metadata():
    _errors(),
    _interfaces(),
    _dict(new MetadataImpl)
{}


Metadata::
Metadata(const Metadata& right):
    _errors(right._errors),
    _interfaces(),
    _dict(new MetadataImpl(*right._dict.get()))
{
    for (auto& iface: right._interfaces)
    {
        _interfaces.emplace_back(iface->clone());
        // _interfaces.back().get()->bind(this);
    }
}


Metadata&
Metadata::
operator=(const Metadata& right)
{
    if (this == &right)
        return *this;
    *_dict = *right._dict;
    _errors = right._errors;
    _interfaces.clear();
    for (auto& iface: right._interfaces)
    {
        _interfaces.emplace_back(iface->clone());
    }
    return *this;
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

Metadata::
~Metadata()
{}


std::string
Metadata::
string_value(const std::string& name)
{
    MetadataImpl& jd = mdict();
    Json::Value value = jd.get(name, Json::Value(""));
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
    wbuilder["indentation"] = std::string(nindent, ' ');
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
    MetadataInterface(classname),
    record_type(this, MetadataItem::READWRITE, "record_type"),
    timestamp(this, MetadataItem::READWRITE, "timestamp"),
    manufacturer(this, MetadataItem::READONLY, "manufacturer", "Manufacturer"),
    model(this, MetadataItem::READONLY, "model", "Model"),
    serial_number(this, MetadataItem::READONLY, "serial_number", "Serial Number"),
    hardware_version(this, MetadataItem::READONLY, "hardware_version", "Hardware Version"),
    manufacture_date(this, MetadataItem::READONLY, "manufacture_date", "Manufacture Date"),
    firmware_version(this, MetadataItem::READONLY, "firmware_version", "Firmware Version"),
    firmware_build(this, MetadataItem::READONLY, "firmware_build", "Firmware Build"),
    calibration_date(this, MetadataItem::READONLY, "calibration_date", "Calibration Date")
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
    _md(),
    _classname(classname),
    _items()
{
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
    DLOG(("adding item '") << item->name() << "' to interface '" << classname() << "'");
    _items.push_back(item);
    return item;
}


Metadata&
MetadataInterface::
metadata()
{
    if (!_md)
        _md = std::make_shared<Metadata>();
    return *_md.get();
}


bool
MetadataInterface::
validate()
{
    return true;
}

MetadataInterface::
~MetadataInterface()
{}


} // namespace core
} // namespace nidas
