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
#ifndef NIDAS_CORE_METADATA_H
#define NIDAS_CORE_METADATA_H

#include <string>
#include <vector>
#include <exception>
#include <limits>
#include <iomanip>

namespace nidas { namespace core {


class Metadata;

// class MetadataException: public std::exception
// {
// public:
//     MetadataException(const std::string& what):
//         std::exception(what)
//     {}
// };


/**
 * MetadataItem is basically a name, description, and string value.
 * 
 * It is meant to be used as a member of a Metadata object, usually defined as
 * a member in a Metadata subclass associated with something like a sensor or
 * event.  However, they also need to be added dynamically, for example to
 * make it possible to define or extend a metadata dictionary from XML.  Thus
 * MetadataItem subclasses mostly add only the typed interface to
 * MetadataItem, but they can also check for valid values.
 * 
 * The MetadataItem name can be unique to a sensor or it can be one of the
 * standard names provided by the Metadata base class.
 *
 * There are different kinds of metadata.  Manufacturing metadata can only be
 * read from the sensor and cannot be changed, or perhaps it might have to be
 * assigned by the config if it cannot be queried.  Sensor configuration is
 * usually those settings which can be queried from the sensor but also
 * changed.  Sensor configurations need to be recorded, but also written to
 * the sensor and verified.
 * 
 * Since changes to metadata likely need to be tracked and validated closely,
 * all value access goes through methods.  Invalid values will not be stored,
 * and errors can be checked with the error() method.
 * 
 * Both manufacturing and configuration metadata are important to recording
 * the state of a sensor.  Rather than try to manage them separately, they can
 * be combined into one Metadata object by differentiating the items
 * themselves.  Thus metadata items can have these "settability" settings:
 *
 * READONLY: The value will be queried from the sensor, but it cannot be
 * changed, such as the serial number.  There may be settings which can
 * technically be changed on a sensor but which should never be changed, in
 * which case they might be marked as READONLY.
 * 
 * USERSET: The user configures the setting, it cannot be queried from the
 * sensor, such as depth, height, color, property number.  This may turn out
 * not to be needed.
 * 
 * READWRITE: The value can be queried from the sensor and also changed in the
 * sensor, meaning it can also be configured in the sensor XML to be installed
 * when the sensor is opened.
 * 
 * Metadata items can have these values:
 *
 * UNSET: It has not been set, not by the user, and not retrieved from the
 * sensor.
 * 
 * FAILED: A read from the sensor was attempted but it failed.  Is this
 * necessary?
 * 
 * <value>: the value of the metadata, stored in string format.
 *
 * MetadataItem implementations can check for valid assignments and accumulate
 * error messages for invalid assignments in an error buffer.  Call error() to
 * retrieve the error message.
 * 
 * Other implementation attempts included a pointer to the containing Metadata
 * dictionary in each MetadataItem.  However, that makes it harder to allow
 * Metadata and MetadataItem to be copy constructed and members of other
 * objects.  So currently there is no way for the MetadataItem to call back to
 * the dictionary to notify of a change and trigger other behavior.
 */
class MetadataItem
{
public:
    enum MODE { READONLY, USERSET, READWRITE };

    static const std::string UNSET;
    static const std::string FAILED;

    enum MODE
    mode() const
    {
        return _mode;
    }

    /**
     * Return true if this item has not been set.
     */
    bool
    unset() const
    {
        return _string_value == UNSET;
    }

    std::string
    name() const
    {
        return _name;
    }

    std::string
    description() const
    {
        return _description;
    }

    std::string
    string_value() const
    {
        return _string_value;
    }

    /**
     * Return the current error message.  If an attempt to assign a new value
     * fails, such as when a set() call in a subclass returns false, the
     * return value will be a non-empty explanation of the error.  Subclasses
     * should clear the error message before each assignment attempt or
     * validity check.
     */
    std::string
    error() const
    {
        return _error;
    }

    /**
     * Reset the value to UNSET.
     */
    void
    erase();

    virtual
    ~MetadataItem();

protected:
    /**
     * MetadataItem assignment means assigning just the value from @p source.
     * The other metadata (like name and description) are not modified.
     * MetadataItem subclasses with no members or only a typed value member
     * can just expose the default assignment method.
     * 
     * The base implementation calls check_assign_string() to assign the string
     * value, since the constraints in this item may be different than in the
     * source item.  This means that if the check fails, error() will be set
     * and this value will not change.
     * 
     * If the source value is unset(), then it is not assigned.  To erase a
     * value, call erase().
     * 
     * There is no check that the other MetadataItem has the same name, since
     * it wouldn't be unusual to want to copy the same value for some metadata
     * into multiple items.
     */
    MetadataItem& operator=(const MetadataItem& source);

    /**
     * Copy construction is allowed.  Note that even the current error message
     * from the source is preserved in the copy.  Like assignment, this is
     * protected so that a subclass of MetadataItem cannot be sliced by
     * assignment.
     */
    MetadataItem(const MetadataItem&) = default;

    /**
     * Construct a MetadataItem.  This is protected because there's really no
     * reason to instantiate the base class, since it has no public methods to
     * modify the value.
     */
    MetadataItem(enum MODE mode,
                 const std::string& name,
                 const std::string& description="");

    /**
     * Check if string value @p incoming is valid, and if so, update the
     * string value for this item, possibly reformatted.
     * 
     * If invalid, do not change the item value, but set a message in string
     * @p error.  When valid, @p error will be unchanged, so it is up to the
     * caller to make sure it is empty if that's how an error will be
     * detected.  The error string in the interface allows implementations to
     * return a meaningful message, but if an implementation thinks an invalid
     * setting is important enough, it could choose to throw that message in
     * an exception.
     * 
     * The reformat allows the MetadataItem to control how valid values are
     * formatted when stored as a string.  For example, setting a string value
     * of "3.1415927" to a float would be stored as "3.14" if a precision of 3
     * is enforced.
     * 
     * The default implementation in the base class does no checking, it just
     * assigns the string value.
     * 
     * Typed interfaces convert an incoming typed value to a string
     * and then pass it to this method.  Otherwise, if the 
     */
    virtual bool
    check_assign_string(const std::string& incoming);

    // Subclasses call this to change the string value.
    void
    update_string_value(const std::string& value);

    enum MODE _mode;
    std::string _name;
    std::string _description;
    std::string _string_value;
    std::string _error;

};


/**
 * Write string_value() to the stream.
 */
std::ostream& operator<<(std::ostream&, const MetadataItem&);


/**
 * A MetadataItem whose value is just a string.
 */
class MetadataString : public MetadataItem
{
public:
    MetadataString(enum MODE mode,
                   const std::string& name,
                   const std::string& description="");

    std::string
    get();

    operator std::string();

    /**
     * Set the value of this item.  Return true if the setting succeeded,
     * meaning the value was valid, and otherwise false.
     */
    bool
    set(const std::string& value);

    /**
     * Allow direct string assignment.  If the assignment fails because of
     * constraint checks, then error() will be non-empty.
     */
    MetadataString&
    operator=(const std::string& value);

    /**
     * Allow the value of one string item to be assigned to another.
     */
    MetadataString& operator=(const MetadataString&) = default;
};


/**
 * A MetadataItem with string value is "true" or "false".
 */
class MetadataBool : public MetadataItem
{
public:
    MetadataBool(enum MODE mode,
                 const std::string& name,
                 const std::string& description="");

    virtual bool check_assign_string(const std::string& incoming);

    bool get();

    operator bool();

    bool from_string(const std::string& incoming);

    std::string to_string(bool value);

    bool set(bool value);

    /**
     * There is no state in this class to preserve, so only MetatdataItem
     * assignemnt is needed.
     */
    MetadataBool& operator=(const MetadataBool&) = default;

    /**
     * Copy construction is allowed.
     */
    MetadataBool(const MetadataBool&) = default;
};


template <typename T>
class MetadataNumber : public MetadataItem
{
public:
    MetadataNumber(enum MODE mode,
                   const std::string& name,
                   const std::string& description="",
                   int precision_=12,
                   T min_ = std::numeric_limits<T>::min(),
                   T max_ = std::numeric_limits<T>::max());

    T get();

    operator T();

    bool set(const T& value);

    MetadataNumber<T>& operator=(const T& value);

    // There could be an operator==(const T& right) in this class which
    // implicitly calls get(), but that might make it too easy to neglect that
    // this is an object with other properties to compare and not _just_ a
    // number.

    /**
     * Call check_assign_string().
     */
    MetadataNumber<T>& operator=(const std::string& value);

    virtual bool check_assign_string(const std::string& incoming) override;

    std::string to_string(const T& value);

    T from_string(const std::string& value);

    /**
     * Allow the value to be copied from another kind of metadata without
     * copying the precision and limits.
     */
    MetadataNumber<T>& operator=(const MetadataNumber<T>& right);

    /**
     * Allow default copy construction, so that a Metadata dictionary can be
     * copied, which unlike assignment means keeping all the item properties
     * the same, since it's like creating the same type of metadata item from
     * an existing item.
     */
    MetadataNumber(const MetadataNumber& right) = default;

private:

    int precision;
    T min;
    T max;

};


// These will be common, so give them aliases.
using MetadataDouble = MetadataNumber<float>;
using MetadataFloat = MetadataNumber<float>;
using MetadataInt = MetadataNumber<int>;


/**
 * Metadata is a MetadataItem dictionary.
 * 
 * The idea is to implement a kind of reflection so that metadata can be
 * managed like a dictionary of properties, serialized to and from string
 * forms, like for JSON, queried and printed, while allowing convenient and
 * safe typed access in subclasses of Metadata for specific sensors or events.
 * 
 * In the most basic sense it is a dictionary of metadata items keyed by a
 * name each with a string value.  It can be translated to and from persistent
 * formats like json, to be stored or exchanged in a human-readable form.
 * 
 * Metadata can contain information read from the sensor and also
 * configuration settings that need to be written to the sensor and verified.
 * A MetadataItem identified as readwrite can be used to verify that settings
 * have been applied to a sensor successfully.  For example, a Metadata 
 * subclass can add members for a specific kind of sensor, and then use this
 * algorithm to test that settings were applied:
 * 
 * - Create a Metadata object.
 * 
 * - Query the sensor and record the settings in the object.
 * 
 * - Compare the object from the sensor with a target configuration.  The
 *   target configuration is an instance of Metadata with some number of
 *   readwrite properties set to specific values.  If any of those explicit
 *   settings differ from the sensor metadata, then the sensor needs to be
 *   updated.
 * 
 * - To update the sensor, the differing settings can be enumerated and
 *   applied individually to the sensor.  Or, the differing members can be
 *   applied to the metadata object from the sensor, and then the metadata can
 *   be applied to the sensor, specifically all the properties which are
 *   readwrite and can be changed.
 * 
 * Essentially the same algorithm works to verify that a sensor configuration
 * still matches the target.
 * 
 * - Create a Metadata object.
 * 
 * - Query the sensor and record the settings in the object.
 * 
 * - Compare the sensor metadata with a copy of the previous sensor state.
 *   Any difference indicates the sensor or its configuration has changed.
 * 
 * Finally, since the Metadata dictionary for a sensor should comprise whatever
 * properties are useful about a sensor or which could affect the measurements,
 * the Metadata encapsulates all that information and it can be published
 * in human-readable messages and logs.
 * 
 * - Create a Metadata object.
 * 
 * - Query the sensor and record the settings in the object.
 * 
 * - Convert the Metadata to a JSON string and publish it in an event record.
 * 
 * Subclasses can add typed members which are registered with the Metadata
 * base class, allowing generic access to the string values without needing
 * access to the subclass type.
 */
class Metadata
{
    std::string _classname;

public:

    /**
     * Construct a Metadata dictionary with a set of standard items which may
     * or may not be applicable to the subclasses, but they are available to
     * use if relevant.  The @p classname is set by the subclass.
     * 
     * @param classname
     */
    Metadata(const std::string& classname);

    using item_list = std::vector<MetadataItem*>;

    /**
     * Return a list of pointers to all metadata items that belong to this
     * Metadata dictionary.
     */
    item_list get_items();

    std::string classname();

    MetadataString manufacturer;
    MetadataString model;
    MetadataString serial_number;
    MetadataString hardware_version;
    MetadataString manufacture_date;
    MetadataString firmware_version;
    MetadataString firmware_build;
    MetadataString calibration_date;

    /**
     * Return a pointer to the MetadataItem with name @p name.
     * 
     * If @p name is not found, return nullptr.  The item can be modified
     * through the pointer same as if it were accessed as an object member,
     * but if there is a typed interface to the item (ie, the MetadataItem is
     * a subclass), then the pointer would have to be narrowed to that class.
     * The pointer is only valid of course while the Metadata object memory
     * allocation does not change.
     */
    MetadataItem* lookup(const std::string& name);

    virtual ~Metadata();

protected:

    /**
     * Every subclass needs to be able to enumerate pointers to it's metadata
     * item members.
     */
    virtual void enumerate(item_list&) = 0;

    /** XXX
     * There is no easy way to implement copy construction in the base class
     * and all the subclasses, because the pointers to the members need to be
     * enumerated for each new instance.  So prohibit it, but allow
     * assignment.
     */
    Metadata(const Metadata&) = default;

    /**
     * Assignment must be protected to make it harder to slice the type.  It's
     * up to the most derived subclasses to expose the assignment operator, if
     * it makes sense.  The base class takes care of assigning metadata values
     * to each member without changing the pointers in the table.
     * 
     * This does not change the classname or any other metadata, it just
     * assigns the values from the standard metadata members in @p source.
     * This means an unset value in @p source does not change the item value
     * in this dictionary
     */
    Metadata& operator=(const Metadata& source);
};


} // namespace core
} // namespace nidas

#endif // NIDAS_CORE_METADATA_H
