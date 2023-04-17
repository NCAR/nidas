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

#include "Metadata.h"
#include <nidas/util/Logger.h>

#include <iostream>
#include <sstream>
#include <iomanip>

#include <boost/json/src.hpp>
namespace json = boost::json;


namespace nidas { namespace core {

using nidas::util::UTime;

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
MetadataItem(enum MODE mode,
             const std::string& name,
             const std::string& description):
    _mode(mode),
    _name(name),
    _description(description),
    _string_value(UNSET),
    _error()
{
}


MetadataItem&
MetadataItem::
operator=(const MetadataItem& right)
{
    if (!right.unset())
        check_assign_string(right._string_value);
    return *this;
}


bool
MetadataItem::
check_assign_string(const std::string& incoming)
{
    set_error();
    update_string_value(incoming);
    return true;
}


void
MetadataItem::
update_string_value(const std::string& value)
{
    _string_value = value;
    // someday this could be a more elaborate kind of notification.
    DLOG(("") << "updated metadata: " << _name << "=" << _string_value);
}


void
MetadataItem::
set_error(const std::string& msg)
{
    _error = msg;
}


void
MetadataItem::
set_error(const std::ostringstream& buf)
{
    _error = buf.str();
}


void
MetadataItem::
erase()
{
    set_error();
    update_string_value(UNSET);
}


std::ostream&
operator<<(std::ostream& out, const MetadataItem& item)
{
    return (out << item.string_value());
}


MetadataItem::
~MetadataItem()
{

}


// MetadataString implementation

MetadataString::
MetadataString(enum MODE mode,
                const std::string& name,
                const std::string& description):
    MetadataItem(mode, name, description)
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
MetadataBool(enum MODE mode,
                const std::string& name,
                const std::string& description):
    MetadataItem(mode, name, description)
{}

bool
MetadataBool::
check_assign_string(const std::string& incoming)
{
    bool target = from_string(incoming);
    if (!error().empty())
        return false;
    update_string_value(to_string(target));
    return true;
}

bool
MetadataBool::
get()
{
    if (unset())
        return false;
    return from_string(string_value());
}

MetadataBool::
operator bool()
{
    return get();
}

bool
MetadataBool::
from_string(const std::string& incoming)
{
    set_error();
    bool target{false};
    if (incoming == "true")
        target = true;
    else if (incoming == "false")
        target = false;
    else
    {
        set_error("could not parse as bool: " + incoming);
    }
    return target;
}

std::string
MetadataBool::
to_string(bool value)
{
    return value ? "true" : "false";
}

bool
MetadataBool::
set(bool value)
{
    return check_assign_string(to_string(value));
}


// MetadataNumber<T>

template <typename T>
MetadataNumber<T>::
MetadataNumber(enum MODE mode,
               const std::string& name,
               const std::string& description,
               int precision_, T min_, T max_):
    MetadataItem(mode, name, description),
    precision(precision_), min(min_), max(max_)
{}

template <typename T>
T
MetadataNumber<T>::
get()
{
    if (unset())
        return T();
    return from_string(string_value());
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
    return check_assign_string(to_string(value));
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
    set_error();
    T target = from_string(incoming);
    if (!error().empty())
    {
        return false;
    }
    if (target < min || max < target)
    {
        set_error(errbuf() << target << " is not in range ["
                           << min << ", " << max << "]");
        return false;
    }
    update_string_value(to_string(target));
    return true;
}

template <typename T>
std::string
MetadataNumber<T>::
to_string(const T& value)
{
    std::ostringstream outb;
    outb << std::setprecision(precision) << value;
    return outb.str();
}

template <typename T>
T
MetadataNumber<T>::
from_string(const std::string& value)
{
    set_error();
    std::istringstream inb(value);
    T target{0};
    if (!(inb >> target))
    {
        set_error(errbuf("could not parse as a number: ") << value);
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
MetadataTime(enum MODE mode,
                const std::string& name,
                const std::string& description):
    MetadataItem(mode, name, description)
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
    set_error();
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
    set_error();
    UTime ut(from_string(incoming));
    if (!error().empty())
        return false;
    update_string_value(ut.to_iso());
    return true;
}

UTime
MetadataTime::
from_string(const std::string& value)
{
    UTime ut(0l);
    if (!ut.from_iso(value))
    {
        set_error("could not parse time: " + value);
    }
    return ut;
};


// Metadata implementation

Metadata::
Metadata(const std::string& classname):
    _classname(classname),
    record_type(MetadataItem::READWRITE, "record_type"),
    timestamp(MetadataItem::READWRITE, "timestamp"),
    manufacturer(MetadataItem::READONLY, "manufacturer", "Manufacturer"),
    model(MetadataItem::READONLY, "model", "Model"),
    serial_number(MetadataItem::READONLY, "serial_number", "Serial Number"),
    hardware_version(MetadataItem::READONLY, "hardware_version", "Hardware Version"),
    manufacture_date(MetadataItem::READONLY, "manufacture_date", "Manufacture Date"),
    firmware_version(MetadataItem::READONLY, "firmware_version", "Firmware Version"),
    firmware_build(MetadataItem::READONLY, "firmware_build", "Firmware Build"),
    calibration_date(MetadataItem::READONLY, "calibration_date", "Calibration Date")
{}


const std::string&
Metadata::
classname()
{
    return _classname;
}


Metadata::item_list
Metadata::
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


MetadataItem*
Metadata::
lookup(const std::string& name)
{
    for (auto mi: get_items())
    {
        if (mi->name() == name)
            return mi;
    }
    return nullptr;
}


Metadata&
Metadata::
operator=(const Metadata& right)
{
    manufacturer = right.manufacturer;
    model = right.model;
    serial_number = right.serial_number;
    hardware_version = right.hardware_version;
    manufacture_date = right.manufacture_date;
    firmware_version = right.firmware_version;
    firmware_build = right.firmware_build;
    calibration_date = right.calibration_date;
    return *this;
}

Metadata::
~Metadata()
{}


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


} // namespace core
} // namespace nidas
