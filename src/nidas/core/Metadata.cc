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

namespace nidas { namespace core {

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
    _error.clear();
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
erase()
{
    _error.clear();
    update_string_value(UNSET);
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
    return _string_value;
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
    if (!_error.empty())
        return false;
    update_string_value(to_string(target));
    return true;
}

bool
MetadataBool::
get()
{
    return from_string(_string_value);
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
    _error.clear();
    bool target{false};
    if (incoming == "true")
        target = true;
    else if (incoming == "false")
        target = false;
    else
    {
        _error = "could not parse as bool: " + incoming;
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
    return from_string(_string_value);
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
    _error.clear();
    T target = from_string(incoming);
    if (!_error.empty())
    {
        return false;
    }
    if (target < min || max < target)
    {
        std::ostringstream msg;
        msg << target << " is not in range [" << min << ", " << max << "]";
        _error = msg.str();
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
    _error.clear();
    std::istringstream inb(value);
    T target{0};
    if (!(inb >> target))
    {
        std::ostringstream msg;
        msg << "could not parse as a number: " << value;
        _error = msg.str();
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

// explicit instantiation of the types with aliases.
template class MetadataNumber<float>;
template class MetadataNumber<double>;
template class MetadataNumber<int>;


// Metadata implementation

Metadata::
Metadata(const std::string& classname):
    _classname(classname),
    manufacturer(MetadataItem::READONLY, "manufacturer", "Manufacturer"),
    model(MetadataItem::READONLY, "model", "Model"),
    serial_number(MetadataItem::READONLY, "serial_number", "Serial Number"),
    hardware_version(MetadataItem::READONLY, "hardware_version", "Hardware Version"),
    manufacture_date(MetadataItem::READONLY, "manufacture_date", "Manufacture Date"),
    firmware_version(MetadataItem::READONLY, "firmware_version", "Firmware Version"),
    firmware_build(MetadataItem::READONLY, "firmware_build", "Firmware Build"),
    calibration_date(MetadataItem::READONLY, "calibration_date", "Calibration Date")
{}


std::string
Metadata::
classname()
{
    return _classname;
}


Metadata::item_list
Metadata::
get_items()
{
    item_list items;
    auto members = {
        &manufacturer,
        &model,
        &serial_number,
        &hardware_version,
        &manufacture_date,
        &firmware_version,
        &firmware_build,
        &calibration_date
    };
    std::copy(begin(members), end(members), std::back_inserter(items));
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


} // namespace core
} // namespace nidas
