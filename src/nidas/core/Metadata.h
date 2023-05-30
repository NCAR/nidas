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

#include <nidas/util/UTime.h>

#include <string>
#include <vector>
#include <memory>
#include <exception>
#include <limits>
#include <iosfwd>



namespace nidas { namespace core {


class MetadataException: public std::runtime_error
{
public:
    MetadataException(const std::string& what);
    MetadataException(const std::ostringstream& buf);
};


class MetadataStore;
class MetadataInterface;
class MetadataString;
class MetadataBool;
class MetadataTime;

template <typename T> class MetadataNumber;

/**
 * Type aliases for the two numeric types.  A separate float type is not
 * needed since double can have a precision.
 */
using MetadataDouble = MetadataNumber<double>;
using MetadataInt = MetadataNumber<int>;


/**
 * MetadataItem is basically a name, description, and a typed interface to set
 * and get named values in a metadata dictionary.  Each item belongs to a
 * MetadataInterface object, through which the item gets access to the
 * dictionary storage.
 *
 * There are different kinds of metadata.  Manufacturing metadata can only be
 * read from the sensor and cannot be changed, or perhaps it might have to be
 * assigned by the config if it cannot be queried.  Sensor configuration is
 * usually those settings which can be queried from the sensor but also
 * changed.  Sensor configurations need to be recorded, but also written to
 * the sensor and verified.
 * 
 * Thus metadata items can have these "settability" settings:
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
 * MetadataItem subclasses implement typed interfaces to set and get the
 * stored string value with different constraints, such as a native type or
 * value limits.  Typically a get() method returns the typed value.  If the
 * value is unset(), then get() returns some default value for the subclass
 * type.  Callers can test unset() to differentiate a default value from an
 * actual set value.  There is usually also a set() method which accepts a
 * typed parameter.  If the typed parameter fails the constraints, then the
 * string value does not change and an error message is added to the metadata.
 * See check_assign_string().
 * 
 * Once a MetadataItem has been assigned a pointer to its containing
 * MetadataInterface instance, that pointer will not be changed by copy
 * construction or by assignment.
 */
class MetadataItem
{
public:
    enum MODE { READONLY, USERSET, READWRITE };

    enum MODE
    mode() const
    {
        return _mode;
    }

    /**
     * Return true if this item has not been set.
     */
    bool
    unset() const;

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

    /**
     * Return the metadata value as text, as might be written to a sensor or a
     * log message.  This is not necessarily the same form as in the backing
     * store.
     */
    virtual std::string
    string_value() const;

    /**
     * Remove this item from the metadata, after which unset() will be true.
     */
    void
    erase();

    virtual
    ~MetadataItem();

protected:
    /**
     * MetadataItem assignment means assigning just the value from @p source.
     * The other metadata (like name and description) are not modified.  If a
     * MetadataItem subclass wants to allow this type of assignment from the
     * same or other item types, and typically if the subclass has no other
     * members anyway, then it can just expose the default assignment method.
     *
     * If the source value is unset(), then it is not assigned.  To erase a
     * value, call erase().
     * 
     * There is no check that the other MetadataItem has the same name, since
     * it is reasonable to copy the same value from one metadata item into
     * multiple items.  However, this is protected to prevent one subclass
     * value from being assigned to a different subclass.  If a subclass wants
     * to allow that, then it can provide a public assignment method accepting
     * other MetadataItem subclass types.
     * 
     * The assigned value is not checked.
     */
    MetadataItem& operator=(const MetadataItem& source);

    /**
     * Default copy construction works but subclasses have to allow it.  Note
     * that even the current error message from the source is preserved in the
     * copy.  Like assignment, this is protected so that a subclass of
     * MetadataItem cannot be sliced by assignment.
     */
    MetadataItem(const MetadataItem&) = delete;

    /**
     * Construct a MetadataItem.  This is protected because there's really no
     * reason to instantiate the base class, since it has no public methods to
     * modify the value.
     */
    MetadataItem(MetadataInterface* mdi, enum MODE mode,
                 const std::string& name,
                 const std::string& description="");

    /**
     * Check if @p incoming is a valid string value, and if so, assign it.
     *
     * Implementations of this method must check that the attempted assignment
     * satisfies the constraints for the values of this item.  Valid values
     * stored as strings may also need to be reformatted into the corrent
     * string format before being passed to update_string_value().
     *
     * If @p incoming is invalid, do not change the item value, but add an
     * error message to the metadata with add_error().  When the value is
     * valid and assignment succeeds, the return value will be true.  The
     * error string allows implementations to return a meaningful message, but
     * if an implementation thinks an invalid setting is important enough, it
     * could choose to throw that message in an exception.
     *
     * The base class implementation does no checking, it just assigns the
     * string value.  If a subclass wants that behavior, it can just call this
     * implementation.
     */
    virtual bool
    check_assign_string(const std::string& incoming);

    /**
     * Subclasses call this to set a string value.
     * 
     * Unlike check_assign_string(), this just updates the string value and
     * does no checks.  This is a convenience method for item subclasses which
     * enforce a schema but store the value as a string type.
     */
    void
    update_string_value(const std::string& value);

    /**
     * Give subclasses access to the MetadataInterface.
     */
    MetadataInterface* mdi();

    /**
     * Return reference to the underlying MetadataStore store for this item, accessed
     * through the owning MetadataInterface.
     */
    MetadataStore& metadata();

private:
    MetadataInterface* _mdi;
    enum MODE _mode;
    std::string _name;
    std::string _description;
};


/**
 * MDITEM macro makes it easier to generate the item initializer by setting
 * the item name to the symbol name, and inserting the common READWRITE and
 * this parameters.
 */
#define MDITEM(name, desc) name{this, READWRITE, #name, desc}


/**
 * MetadataStore are implemented as a key-value dictionary with schema
 * interfaces.  The metadata storage is created and accessed through at least
 * one MetadataInterface, keeping the underlying storage type hidden.
 * 
 * The MetadataStore object holds all the portable state and can be managed
 * like a dictionary of properties, serialized to and from string forms like
 * JSON, queried and printed.  MetadataInterface objects can be attached to
 * the dictionary to provide safe typed access to the dictionary and implement
 * validation, and those interfaces stay with the metadata so it can be
 * validated even when callers do not have access to the actual interface
 * definition.
 * 
 * MetadataStore can contain information read from the sensor and also
 * configuration settings that need to be written to the sensor and verified.
 * A MetadataItem identified as readwrite can be used to verify that settings
 * have been applied to a sensor successfully.  For example, a MetadataStore
 * subclass can add members for a specific kind of sensor, and then use this
 * algorithm to test that settings were applied:
 *
 * - Create a MetadataStore object.  As needed, attach the MetadataInterface
 *   subclass for the sensor to provide typed, named access to the metadata
 *   values.
 *
 * - Query the sensor and record the settings in the object.
 * 
 * - Compare the object from the sensor with a target configuration.  The
 *   target configuration is an instance of MetadataStore with some number of
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
 * - Create a MetadataStore object.
 * 
 * - Query the sensor and record the settings in the object.
 * 
 * - Compare the sensor metadata with a copy of the previous sensor state.
 *   Any difference indicates the sensor or its configuration has changed.
 * 
 * Finally, since the MetadataStore dictionary for a sensor should comprise
 * whatever properties are useful about a sensor or which could affect the
 * measurements, the MetadataStore encapsulates all that information and it
 * can be published in human-readable messages and logs.
 * 
 * - Create a MetadataStore object.
 * 
 * - Query the sensor and record the settings in the object.
 * 
 * - Convert the MetadataStore to a JSON string and publish it in an event
 *   record.
 * 
 * Subclasses can add typed members which are registered with the
 * MetadataStore base class, allowing generic access to the string values
 * without needing access to the subclass type.
 *
 * MetadataInterface adds a schema to a metadata dictionary, allowing named
 * items to be accessed through their typed interfaces and also adding
 * functionality to verify that the dictionary is a valid instance of the
 * given interface.  All interfaces to the metadata storage are attached to
 * the storage and carried with it.
 */
class MetadataInterface
{
public:

    // Import item mode settings.
    static const auto READONLY = MetadataItem::READONLY;
    static const auto USERSET = MetadataItem::USERSET;
    static const auto READWRITE = MetadataItem::READWRITE;

    // Aliases for item types so subclasses do not need to qualify them.
    using MetadataString = nidas::core::MetadataString;
    using MetadataBool = nidas::core::MetadataBool;
    using MetadataDouble = nidas::core::MetadataDouble;
    using MetadataInt = nidas::core::MetadataInt;
    using MetadataTime = nidas::core::MetadataTime;

    /**
     * Construct a MetadataInterface with no schema and its own copy of an
     * empty MetadataStore dictionary.
     */
    MetadataInterface(const std::string& classname = "");

    /**
     * Return true if the metadata dictionary has valid settings for this interface.
     * If false, then there will be error messages explaining the failures.
     */
    virtual bool validate();

    /**
     * Set metadata with @p name to string @p value.
     * 
     * This does not check any constraints and will overwrite any value
     * already set by an interface attached to the metadata storage.
     */
    void set(const std::string& name, const std::string& value);

    /**
     * Return true if there is no metadata item named @p name.
     */
    bool unset(const std::string& name);

    virtual std::string
    to_buffer(int indent=0);

    virtual bool
    from_buffer(const std::string& buffer);

    /**
     * Return the value as a string for the metadata with name @p name.
     * 
     * Unset values are returned as an empty string.
     */
    virtual std::string
    string_value(const std::string& name);

    using item_list = std::vector<MetadataItem*>;

    /**
     * Return a list of pointers to all metadata items that belong to this
     * MetadataStore dictionary.
     */
    item_list get_items();

    /**
     * Return a pointer to the MetadataItem with name @p name.
     * 
     * If @p name is not found, return nullptr.  The item can be modified
     * through the pointer same as if it were accessed as an object member,
     * but if there is a typed interface to the item (ie, the MetadataItem is
     * a subclass), then the pointer would have to be narrowed to that class.
     * The pointer is only valid of course while the MetadataStore object
     * memory allocation does not change.
     */
    MetadataItem* lookup(const std::string& name);

    MetadataInterface*
    get_interface(const std::string& name);

    /**
     * Allocate a new MetadataInterface of type T and add it to this
     * MetadataStore object.  The interface must be dynamically allocated so
     * it can be owned by this instance and cloned when this object is copied.
     * Assign the return value to keep a reference to this interface backed by
     * this MetadataStore.
     */
    template <typename T>
    T* add_interface(const T& prototype)
    {
        MetadataInterface* found = get_interface(prototype.classname());
        T* mdi = dynamic_cast<T*>(found);
        if (!mdi)
        {
            mdi = dynamic_cast<T*>(prototype.clone());
            attach_interface(mdi);
        }
        return mdi;
    }

    /**
     * Return a reference to the MetadataStore dictionary bound for this
     * interface.  If there is no dictionary yet, then this will allocate one.
     */
    MetadataStore& metadata();

    /**
     * Clone this interface onto a different MetadataStore object.
     */
    virtual MetadataInterface*
    clone() const = 0;

    std::string
    classname() const { return _classname; }

    virtual ~MetadataInterface();

    /**
     * Copy construction is disabled.  All the MetadataItem members of
     * interface subclasses needed to be initialized in the default
     * constructor, and rather than complicate matters by trying to replicate
     * that initialization for the copy constructors, just require assignment
     * instead of copying.
     */
    MetadataInterface(const MetadataInterface& right) = delete;

    /**
     * Assign the metadata from interface @p right, but do not assign the item
     * list, since those pointers are to the item instances created and
     * contained by this interface.  Subclass interfaces typically can use
     * default assignment methods, because MetadataItem implements assignment
     * but do not change their interface pointer.
     *
     * It is ok to assign from a subclass type.  The righthand interface is
     * added this interface, whether it's the same interface as this interface
     * or not.
     *
     * Also, since the MetadataStore dictionary is copied here, it may seem
     * unnecessary to do the memberwise assignment of the MetadataItem members
     * in the subclasses.  Typically there will be no difference.  However,
     * the memberwise assignments and copies do allow a MetadataItem subclass
     * to enforce schema constraints on the copy.
     *
     * @return this interface
     */
    MetadataInterface& operator=(const MetadataInterface& right);

    /**
     * Allow any interface type to be assigned to another interface type.
     * 
     * This is the same as the assignment operator, but it can assign any
     * interface to this interface.  It is implemented by erasing the existing
     * metadata and then merging.
     * 
     * @return this interface
     */
    virtual MetadataInterface& assign(const MetadataInterface& right);

    /**
     * Apply all the values and interfaces from the @p right interface,
     * without erasing any values in this metadata.
     * 
     * @return this interface
     */
    virtual MetadataInterface& merge(const MetadataInterface& right);

private:

    /**
     * Add the interface to our metadata and bind it
     * 
     * @param mdi 
     * @return MetadataInterface* 
     */
    MetadataInterface*
    attach_interface(MetadataInterface* mdi);

    /**
     * Replace the storage backend for this interface.  Any existing metadata
     * values are lost.  MetadataStore objects call this to replace any
     * existing binding when attaching an interface.  Thus the friend access
     * for the MetadataStore class.
     */
    void
    bind(MetadataStore* md);

    friend class MetadataStore;

    /**
     * Register an item with this interface.  this is assumed to be a value
     * member of this object, so the pointer will not be released, it will
     * just be added to the list of items which can be iterated.
     */
    MetadataItem*
    add_item(MetadataItem* item);

    friend class MetadataItem;

    // Each MetadataInterface can own it's own MetadataStore backing store, or
    // it can shared the MetadataStore dictionary with another interface, in
    // which case the lifetime of this interface is tied to that
    // MetadataStore.  _md is the MetadataStore storage used by this instance,
    // and if that instance belongs to this object, then it is managed by
    // _owned_md.
    MetadataStore* _md;
    std::unique_ptr<MetadataStore> _owned_md;
    std::string _classname;
    item_list _items;
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
    MetadataString(MetadataInterface* mdi, enum MODE mode,
                   const std::string& name,
                   const std::string& description="");

    /**
     * Return the string value or else an empty string if unset.
     * 
     * Call unset() to check if the string value was explicitly set to an
     * empty string.
     */
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
     * constraint checks, then an error will be added to the metadata.
     */
    MetadataString&
    operator=(const std::string& value);
};


/**
 * A MetadataItem with string value is "true" or "false".
 */
class MetadataBool : public MetadataItem
{
public:
    MetadataBool(MetadataInterface* mdi, enum MODE mode,
                 const std::string& name,
                 const std::string& description="");

    /**
     * If the string @p incoming can be converted to a bool by from_string(),
     * convert the bool value to a string with to_string() and set the value
     * of this item to that string.
     */
    virtual bool check_assign_string(const std::string& incoming) override;

    /**
     * Return the value of this item as a bool, or false if unset().
     */
    bool get();

    operator bool();

    bool set(bool value);

    MetadataBool& operator=(bool value);
};


template <typename T>
class MetadataNumber : public MetadataItem
{
public:
    MetadataNumber(MetadataInterface* mdi, enum MODE mode,
                   const std::string& name,
                   const std::string& description="",
                   int precision_=12,
                   T min_ = std::numeric_limits<T>::lowest(),
                   T max_ = std::numeric_limits<T>::max());

    /**
     * Return the value or T() if unset().
     */
    T get() const;

    operator T();

    bool set(const T& value);

    MetadataNumber<T>& operator=(const T& value);

    // There could be an operator==(const T& right) in this class which
    // implicitly calls get(), but that might make it too easy to neglect that
    // this is an object with other properties to compare and not _just_ a
    // number.

    /**
     * Call check_assign_string() with @p value.
     */
    MetadataNumber<T>& operator=(const std::string& value);

    virtual bool check_assign_string(const std::string& incoming) override;

    std::string to_string(const T& value) const;

    bool from_string(T& value, const std::string& text);

    /**
     * Return the string value with the specified precision.
     */
    std::string string_value() const override;

    /**
     * Allow the value to be copied from another kind of metadata without
     * copying the precision and limits.
     */
    MetadataNumber<T>& operator=(const MetadataNumber<T>& right);

private:

    int precision;
    T min;
    T max;

};



/**
 * MetadataTime is a time stored as a string in ISO8601 format.
 */
class MetadataTime: public MetadataItem
{
public:
    using UTime = nidas::util::UTime;

    MetadataTime(MetadataInterface* mdi, enum MODE mode,
                 const std::string& name,
                 const std::string& description="");

    /**
     * Return the value as a UTime, or if unset(), return UTime(0l).
     */
    UTime get();

    bool set(const UTime& value);

    MetadataTime& operator=(const UTime& ut);

    MetadataTime& operator=(const std::string& value);

    virtual bool check_assign_string(const std::string& incoming) override;

    bool from_string(UTime& ut, const std::string& value);
};


class Metadata: public MetadataInterface
{
public:
    Metadata(const std::string& classname = ""):
        MetadataInterface(classname)
    {};

    virtual MetadataInterface*
    clone() const override
    {
        return new Metadata(classname());
    }
};


/**
 * SensorMetadata is a MetadataInterface for adding metadata typical for a
 * sensor.  Not all of them will be used by all sensors, but it is convenient
 * to group the most common ones in this common interface.  Sensors can add
 * this interface to their metadata or subclass it.
 */
class SensorMetadata: public MetadataInterface
{
public:
    SensorMetadata(const std::string& classname = "sensor");

    MetadataString record_type{this, READWRITE, "record_type"};
    MetadataTime timestamp{this, READWRITE, "timestamp"};
    MetadataString manufacturer{this, READONLY, "manufacturer", "Manufacturer"};
    MetadataString model{this, READONLY, "model", "Model"};
    MetadataString serial_number{this, READONLY, "serial_number", "Serial Number"};
    MetadataString hardware_version{this, READONLY, "hardware_version", "Hardware Version"};
    MetadataString manufacture_date{this, READONLY, "manufacture_date", "Manufacture Date"};
    MetadataString firmware_version{this, READONLY, "firmware_version", "Firmware Version"};
    MetadataString firmware_build{this, READONLY, "firmware_build", "Firmware Build"};
    MetadataString calibration_date{this, READONLY, "calibration_date", "Calibration Date"};

    virtual MetadataInterface*
    clone() const override
    {
        return new SensorMetadata(classname());
    }
};

} // namespace core
} // namespace nidas

#endif // NIDAS_CORE_METADATA_H
