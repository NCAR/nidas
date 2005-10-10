/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_PROJECT_H
#define DSM_PROJECT_H

#include <DOMable.h>
#include <SensorCatalog.h>
#include <ObsPeriod.h>

#include <list>

namespace dsm {

class Site;

/**
 */
class Project : public DOMable {
public:
    Project();
    virtual ~Project();

    static Project* getInstance();

    void setName(const std::string& val) { name = val; }
    const std::string& getName() const { return name; }

    void setCurrentObsPeriod(const ObsPeriod& val) { currentObsPeriod = val; }
    const ObsPeriod& getCurrentObsPeriod() const { return currentObsPeriod; }

    void setVersion(const std::string& val) { version = val; }
    const std::string& getVersion() const { return version; }

    void setXMLName(const std::string& val) { xmlName = val; }
    const std::string& getXMLName() const { return xmlName; }

    void addSite(Site* val) { sites.push_back(val); }
    const std::list<Site*>& getSites() const { return sites; }

    /**
     * When the Project configuration is being used
     * for a specific Site, the value of the current site
     * can be set with setCurrentSite().  Other objects can
     * then have access to the current Site via the
     * getCurrentSite() method of the Project singleton.
     * This may not be the best way to implement this -
     * perhaps we could provide access to the current Site
     * via static methods in the Site class.
     */
    void setCurrentSite(const Site* val) { currentSite = val; }

    const Site* getCurrentSite() const { return currentSite; }

    void setSensorCatalog(SensorCatalog* val) { catalog = val; }
    SensorCatalog* getSensorCatalog() const { return catalog; }

    void fromDOMElement(const xercesc::DOMElement*)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
    		throw(xercesc::DOMException);

    /**
     * Create the full path name of a configuration file from
     * pieces. The path name is created by simply putting
     * a '/' between the value of each of the arguments:
     *      root/projectsDir/project/site/siteSubDir/obsPeriod/fileName
     *
     * This supports configurations in a tree structure looking like so
     * (where directories have a trailing slash):
     *
     *	/root/
     *	  projects/
     *      PROJECT1/
     *        aircraft1/
     *          flights/
     *            flight1/
     *              config.xml
     *            flight2/
     *              config.xml
     *        valley_site_1/
     *          configs/
     *            test/
     *              met.xml
     *            may_june/
     *              met.xml
     *      PROJECT2/
     *        aircraft1/
     *	    ...
     *
     *
     * If root,project,site or obsPeriod begin with a dollar sign,
     * they are treated as environment variables, and their
     * value is fetched from the process environment.
     * If they are not found in the environment, an
     * InvalidParameterException is thrown.
     *
     * @param root Root path of configuration tree, typically
     *    a directory path starting with a '/', like
     *	  "/home/data_sys", or an environment variable
     *    like  $DATASYS_CONFIG, with a value of "/home/data_sys".
     * @param projectsDir Directory path under root where
     *    project configuration files are stored, typically
     *	  something like "projects".
     * @param project Name of project.
     * @param site Name of measurement site, like an aircraft name,
     *    or surface measurement site.
     * @param siteSubDir Directory under the site name where
     *    config files for the observation periods are kept,
     *    like "flights", or "configs".
     * @param obsPeriod  Name of observation period, like
     *    "flight1", "joes_calibration" or "operations".
     * @param fileName  Finally, the name of the file, e.g. "config.xml".
     *     
     */
    static std::string getConfigName(const std::string& root,
    	const std::string& projectsDir,
    	const std::string& project, const std::string& site,
	const std::string& siteSubDir,const std::string& obsPeriod,
	const std::string& fileName)
	throw(atdUtil::InvalidParameterException);

protected:
    static Project* instance;

    std::string name;

    std::string version;

    /**
     * Name of XML file that this project was initialized from.
     */
    std::string xmlName;

    std::list<Site*> sites;

    const Site* currentSite;

    SensorCatalog* catalog;

    ObsPeriod currentObsPeriod;
};

}

#endif
