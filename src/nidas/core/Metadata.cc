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


MetadataException::
MetadataException(const std::string& what):
    std::runtime_error(what)
{}

MetadataException::
MetadataException(const std::ostringstream& buf):
    std::runtime_error(buf.str())
{}


class MetadataStore
{
    std::vector<std::string> _errors;
    std::vector< std::unique_ptr<MetadataInterface> > _interfaces;

    // This is the actual storage, using pimpl idiom to hide the json api.
    Json::Value _dict;

public:

    /**
     * Return the current error messages.
     * 
     * Error messages are added to this metadata when item assignments or
     * interface validations fail.  The messages can be cleared with
     * reset_errors(), such as to test if any errors are reported after a
     * specific assignment or validation.
     */
    const std::vector<std::string>&
    errors() const
    {
        return _errors;
    }

    void
    reset_errors()
    {
        _errors.clear();
    }

    /**
     * Set the error message to @p msg.
     * 
     * The default value of @p msg clears the error message.
     */
    void
    add_error(const std::string& msg="")
    {
        _errors.emplace_back(msg);
    }

    void
    add_error(const std::ostringstream& buf)
    {
        _errors.emplace_back(buf.str());
    }

    /**
     * A shorter type alias to build error messages with std::ostringstream,
     * like set_error(errbuf() << msg << parameter);
     */
    using errbuf = std::ostringstream;

    /**
     * Construct a MetadataStore dictionary.  It has no items and no
     * interfaces, it starts out as an anonymous dictionary with no keys and
     * no values.
     */
    MetadataStore();

    /**
     * Return the value of the given property name as a string.
     * 
     * This does not go through an interface, this is the string form of the
     * value taken directly from the metadata dictionary.  If the name is not
     * set, return an empty string.
     */
    std::string
    string_value(const std::string& name);

    /**
     * Serialize the MetadataStore object to ostream @p out in JSON format.
     * 
     * If @p indent is zero, return a single line of JSON, as appropriate for
     * line-delimited JSON messages, but without a trailing newline.
     * Otherwise return multiple lines using the given amount of indentation,
     * as might be appropriate for writing to a file, also without a trailing
     * newline.
     */
    std::string
    to_buffer(int indent=0);

    /**
     * Parse a metadata dictionary from @p buffer in json format.
     *
     * This automatically resets the errors before parsing and replaces all of
     * the values in the current dictionary.  The new metadata values
     * themselves are not checked in any way.  The json document preserves
     * comments and other settings, even if not associated with any of the
     * interfaces attached to the metadata.
     *
     * @return true if the parse succeeded, false otherwise.
     */
    bool
    from_buffer(const std::string& buffer);

    ~MetadataStore();

    /**
     * Construct MetadataStore by copying.  The metadata dictionary and errors are
     * copied, but the interfaces are not.
     */
    MetadataStore(const MetadataStore& right);

    /**
     * Assigning from another MetadataStore is like copy construction, except
     * any matching interfaces already attached to this MetadataStore are not
     * replaced with clones from the @p source, in case there are already
     * references to those interfaces outside this object.
     */
    MetadataStore& operator=(const MetadataStore& source);

    /**
     * Add the given interface to this MetadataStore object.  This object
     * takes ownership and will delete the interface when it is destroyed.
     */
    MetadataInterface*
    add_interface(MetadataInterface* mdi);

    /**
     * Return the interface matching the given name, or else nullptr.
     */
    MetadataInterface*
    get_interface(const std::string& name);

    /**
     * Find a matching interface return a pointer to it, or else nullptr.  The
     * class name must match and the interface must implement the given class.
     */
    template <typename T>
    T* get_interface(const T& prototype)
    {
        T* found = dynamic_cast<T*>(get_interface(prototype.classname()));
        return found;
    }

    typedef std::vector<MetadataInterface*> interface_list;

    /**
     * Return a vector of pointers to all the interfaces attached to this
     * MetadataStore object.  The pointers are still owned by this object and
     * will be invalid when the object is destroyed.
     */
    interface_list
    interfaces();

    /**
     * Get a reference to the dictionary storage.  Only items use this.
     */
    Json::Value& mdict() { return _dict; }

};



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


MetadataStore&
MetadataItem::
metadata()
{
    return mdi()->metadata();
}


bool
MetadataItem::
unset() const
{
    Json::Value& jd = const_cast<MetadataItem*>(this)->metadata().mdict();
    // an item is unset if it is not a member or set to null.
    return jd.get(_name, Json::Value()).isNull();
}

void
MetadataItem::
erase()
{
    // this is a little ambiguous in that the key could either be set to null
    // or removed to erase it, but this explicitly removes it.
    Json::Value& jd = metadata().mdict();
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
        Json::Value& jd = metadata().mdict();
        Json::Value& jd2 = const_cast<MetadataItem*>(&right)->metadata().mdict();
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
    Json::Value& jd = metadata().mdict();
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
    MetadataStore& md = const_cast<MetadataItem*>(this)->mdi()->metadata();
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
    Json::Value& jd = metadata().mdict();
    jd[name()] = Json::Value(target);
    return true;
}

bool
MetadataBool::
get()
{
    Json::Value& jd = metadata().mdict();
    return jd.get(name(), Json::Value(false)).asBool();
}

MetadataBool::
operator bool()
{
    return get();
}

bool
MetadataBool::
set(bool value)
{
    Json::Value& jd = metadata().mdict();
    jd[name()] = Json::Value(value);
    return true;
}


MetadataBool&
MetadataBool::
operator=(bool value)
{
    set(value);
    return *this;
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
    Json::Value& jd = 
        const_cast<MetadataNumber<T>*>(this)->metadata().mdict();
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
        MetadataStore& md = mdi()->metadata();
        md.add_error(errbuf() << value << " is not in range ["
                           << min << ", " << max << "]");
        return false;
    }
    Json::Value& jd = metadata().mdict();
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
    MetadataStore& md = metadata();
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
    MetadataStore& md = mdi()->metadata();
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
    MetadataStore& md = mdi()->metadata();
    if (!ut.from_iso(value))
    {
        md.add_error("could not parse time: " + value);
        return false;
    }
    return true;
};


// MetadataStore implementation

MetadataStore::
MetadataStore():
    _errors(),
    _interfaces(),
    _dict()
{
    DLOG(("") << "MetadataStore constructor");
}


MetadataStore::
~MetadataStore()
{
    DLOG(("") << "MetadataStore destructor");
}


MetadataStore::
MetadataStore(const MetadataStore& right):
    _errors(right._errors),
    _interfaces(),
    _dict(right._dict)
{
    // The interfaces are not copied here, since they need to be bound to this
    // MetadataStore instance through it's shared pointer.  That happens in
    // the MetadataInterface assignment method.
}


MetadataStore&
MetadataStore::
operator=(const MetadataStore& right)
{
    if (this == &right)
        return *this;
    _dict = right._dict;
    _errors = right._errors;
    for (auto& iface: right._interfaces)
    {
        if (!get_interface(iface->classname()))
            _interfaces.emplace_back(iface->clone());
    }
    return *this;
}


MetadataInterface*
MetadataStore::
add_interface(MetadataInterface* mdi)
{
    _interfaces.emplace_back(mdi);
    return mdi;
}


MetadataInterface*
MetadataStore::
get_interface(const std::string& name)
{
    for (auto& iface: _interfaces)
    {
        if (iface->classname() == name)
            return iface.get();
    }
    return nullptr;
}


MetadataStore::interface_list
MetadataStore::
interfaces()
{
    interface_list interfaces;
    for (auto& iface: _interfaces)
    {
        interfaces.push_back(iface.get());
    }
    return interfaces;
}


std::string
MetadataStore::
string_value(const std::string& name)
{
    Json::Value& jd = mdict();
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
MetadataStore::
to_buffer(int nindent)
{
    Json::StreamWriterBuilder wbuilder;
    wbuilder[std::string("indentation")] = std::string(nindent, ' ');
    // since this is the default precision for number items.
    wbuilder[std::string("precision")] = 12;
    return Json::writeString(wbuilder, _dict);
}


bool
MetadataStore::
from_buffer(const std::string& buffer)
{
    Json::CharReaderBuilder rbuilder;
    std::string errs;
    std::istringstream inbuf(buffer);
    bool ok = Json::parseFromStream(rbuilder, inbuf, &_dict, &errs);
    if (!ok)
    {
        add_error(errs);
        PLOG(("json parse failed: ") << buffer);
    }
    return ok;
}


SensorMetadata::
SensorMetadata(const std::string& classname):
    MetadataInterface(classname)
{}


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


MetadataInterface&
MetadataInterface::
operator=(const MetadataInterface& right)
{
    return assign(right);
}


MetadataInterface&
MetadataInterface::
assign(const MetadataInterface& right)
{
    MetadataStore& mdr = const_cast<MetadataInterface*>(&right)->metadata();
    MetadataStore& mdl = metadata();
    if (&mdr == &mdl)
        return *this;

    mdl.mdict().clear();

    // assignment is just a merge after an erase
    return merge(right);
}


MetadataInterface&
MetadataInterface::
merge(const MetadataInterface& right)
{
    MetadataStore& mdr = const_cast<MetadataInterface*>(&right)->metadata();
    MetadataStore& mdl = metadata();
    if (&mdr == &mdl)
        return *this;

    // note this approach will not copy comments that might be embedded in the
    // right doc, in case that's ever needed.
    Json::Value::Members members = mdr.mdict().getMemberNames();
    for (auto& name: members)
    {
        Json::Value& value = mdr.mdict()[name];
        DLOG(("") << "merging " << name << " from " << right.classname()
                  << " to " << classname() << ": " << value);
        mdl.mdict()[name] = value;
    }

    // now copy any additional interfaces from the right MetadataStore and bind
    // them to the MetadataStore in this instance.
    for (auto& iface: mdr.interfaces())
    {
        add_interface(*iface);
    }

    // because the interface being copied may have been the actual
    // instance type and not attached to the metadata yet, make a point of
    // attaching it to this copy.
    add_interface(right);
    return *this;
}


void
MetadataInterface::
set(const std::string& name, const std::string& value)
{
    metadata().mdict()[name] = value;
}


bool
MetadataInterface::
unset(const std::string& name)
{
    return !metadata().mdict().isMember(name);
}


std::string
MetadataInterface::
to_buffer(int indent)
{
    return metadata().to_buffer(indent);
}


bool
MetadataInterface::
from_buffer(const std::string& buffer)
{
    return metadata().from_buffer(buffer);
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


MetadataInterface*
MetadataInterface::
get_interface(const std::string& name)
{
    auto iface = metadata().get_interface(name);
    DLOG(("") << classname() << ": get_interface(" << name
              << ") returning " << iface);
    return iface;
}


MetadataInterface*
MetadataInterface::
attach_interface(MetadataInterface* mdi)
{
    auto& md = metadata();
    mdi->bind(&md);
    md.add_interface(mdi);
    return mdi;
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
bind(MetadataStore* md)
{
    // this interface may already have it's own metdata store if the
    // constructor sets items, so those need to be merged into the new storage
    // as if they'd been made on that one, ie, as if this interface were using
    // that storage from the beginning of construction.
    DLOG(("") << classname() << " binding new metadata");
    if (_md)
    {
        // can't use Value::copy() here because that replaces everything in
        // the target, so copy memberwise.
        Json::Value::Members members = _md->mdict().getMemberNames();
        for (auto& name: members)
        {
            DLOG(("") << "copying " << name);
            md->mdict()[name] = _md->mdict()[name];
        }
    }
    _md = md;
    // may as well delete here anything that was owned
    _owned_md.reset();
}


MetadataStore&
MetadataInterface::
metadata()
{
    if (!_md)
    {
        DLOG(("") << classname() << ": creating metadata");
        _md = new MetadataStore();
        _owned_md.reset(_md);
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
