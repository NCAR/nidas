/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-05-23 11:30:55 -0700 (Tue, 23 May 2006) $

    $LastChangedRevision: 3364 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/apps/ck_config.cc $
 ********************************************************************
*/

#include <nidas/core/ProjectConfigs.h>

#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

class ProjConfigIO 
{

public:

    ProjConfigIO();

    int parseRunstring(int argc, char** argv);

    static int usage(const char* argv0);

    int run();

    void listConfigs() throw(nidas::core::XMLException,
        n_u::InvalidParameterException);

    void addConfig() throw(nidas::core::XMLException,
        n_u::InvalidParameterException,n_u::IOException);

    void termConfig() throw(nidas::core::XMLException,
        n_u::InvalidParameterException,n_u::IOException);

    enum tasks { NUTTIN_TO_DO, LIST_CONFIGS, ADD_CONFIG, TERM_CONFIG };

private:

    /**
     * XML file containing project configurations.
     */
    string xmlFile;

    /**
     * What to do, per runstring arguments.
     */
    enum tasks task;

    /**
     * Name of project configuration.
     */
    string cname;

    /**
     * Time of start of project configuration
     */
    n_u::UTime cstart;  

    /**
     * Time of end of project configuration
     */
    n_u::UTime cend;  

    /**
     * Name of xml containing nidas configuration for a
     * project.
     */
    string cxml;  

};

/* static */
int ProjConfigIO::usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << " [-l] [-a ... ] [-t ... ] configs_xml_file\n\
-l: list configuration names in configs_xml_file\n\
-a: add a configuration entry to configs_xml_file\n\
-t: terminate a configuration entry in configs_xml_file\n\
\n\
Syntax:\n\
-a name xmlfile \"start\" [\"end\"]]\n\
    Add a configuration entry to the configs_xml_file with a given name,\n\
    with the given XML file name, starting at the time \"start\" and\n\
    ending at the time \"end\". If \"end\" is not given it defaults to\n\
    \"start\" plus two years.\n\
-t \"end\"\n\
    Terminate the last configuration entry in configs_xml_file\n\
    with the given end time\n\
\n\
Start and end times should be enclosed in quotes so they are seen\n\
as one argument.\n\
\n\
Examples:\n\
Add a configuration called \"rf07\", with an XML file named research.xml, starting at 2007 Jan 13 13:40 GMT\n" <<
argv0 << " -a rf07 \'$CONFIGS/research.xml\' \"2007 Jan 13 13:40\"\n\
\n\
Terminate the above configuration at 2007 Jan 14 01:40\n" <<
argv0 << " -t \"2007 Jan 14 01:40\"\n\
\n";
    
    return 1;
}

ProjConfigIO::ProjConfigIO():
    task(NUTTIN_TO_DO)
{
}

int ProjConfigIO::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "a:lt:")) != -1) {

#ifdef DEBUG
        cerr << "opt_char=" << (char) opt_char << ", optind=" << optind <<
            ", argc=" << argc << ", optarg=" << (optarg ? optarg : "") << endl;
        if (optind < argc) cerr << "argv[optind]=" << argv[optind] << endl;
#endif

	switch (opt_char) {
        case 'a':   // add config
            // args: name, xml, start, [end]
            //  -a tf01 "2006 aug 16 00:30" $XXX/$YYY/config.xml
            task = ADD_CONFIG;
            cname = optarg;

            if (optind == argc || argv[optind][0] == '-')
                return usage(argv[0]);
            cxml = argv[optind++];

            if (optind == argc || argv[optind][0] == '-')
                return usage(argv[0]);
            try {
                cstart = n_u::UTime::parse(true,argv[optind++]);
            }
            catch (const n_u::ParseException& e) {
                cerr << e.what() << endl;
                return 1;
            }
            cend = cstart + USECS_PER_DAY * 365 * 2;

            // end time is optional
            if (optind == argc || argv[optind][0] == '-') break;

            try {
                cend = n_u::UTime::parse(true,argv[optind++]);
            }
            catch (const n_u::ParseException& e) {
                cerr << e.what() << endl;
                return 1;
            }
            // cerr << "end=" << cend << endl;
            break;
	case 'l':
	    task = LIST_CONFIGS;
	    break;
	case 't':   // terminate last config
            // args: name, start, end, xml
            //  -e "2006 aug 16 00:30"
	    task = TERM_CONFIG;
            try {
                cend = n_u::UTime::parse(true,optarg);
            }
            catch (const n_u::ParseException& e) {
                cerr << e.what() << endl;
                return 1;
            }
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    if (task == NUTTIN_TO_DO) return usage(argv[0]);
    if (optind < argc) xmlFile = argv[optind++];
    if (xmlFile.length() == 0) return usage(argv[0]);
    if (optind != argc) return usage(argv[0]);
    return 0;
}

int ProjConfigIO::run() 
{
    try {
        switch(task) {
        case LIST_CONFIGS:
            listConfigs();
            break;
        case ADD_CONFIG:
            addConfig();
            break;
        case TERM_CONFIG:
            termConfig();
            break;
        default:
            return 1;
        }
        return 0;
    }
    catch(const nidas::core::XMLException& e) {
        cerr << e.what();
        return 1;
    }
    catch(const n_u::InvalidParameterException& e) {
        cerr << e.what();
        return 1;
    }
    catch(const n_u::IOException& e) {
        cerr << e.what();
        return 1;
    }
}

void ProjConfigIO::listConfigs()
    throw(nidas::core::XMLException,
        n_u::InvalidParameterException)
{
    ProjectConfigs configs;
    configs.parseXML(xmlFile);

    const list<const ProjectConfig*>& cfgs = configs.getConfigs();

    list<const ProjectConfig*>::const_iterator ci = cfgs.begin();

    for( ; ci != cfgs.end(); ++ci) {
        const ProjectConfig* cfg = *ci;
        cout << cfg->getName() << endl;
    }
}

void ProjConfigIO::addConfig()
    throw(nidas::core::XMLException,
        n_u::InvalidParameterException,n_u::IOException)
{
    ProjectConfigs configs;
    configs.parseXML(xmlFile);

    ProjectConfig* ncfg = new ProjectConfig;
    ncfg->setName(cname);
    ncfg->setBeginTime(cstart);
    ncfg->setEndTime(cend);
    ncfg->setXMLName(cxml);

    configs.addConfig(ncfg);

#ifdef DEBUG
    const list<const ProjectConfig*>& cfgs = configs.getConfigs();
    list<const ProjectConfig*>::const_iterator ci = cfgs.begin();
    for( ; ci != cfgs.end(); ++ci) {
        const ProjectConfig* cfg = *ci;
        cout << cfg->getName() << endl;
    }
#endif

    configs.writeXML(xmlFile);
}

void ProjConfigIO::termConfig()
    throw(nidas::core::XMLException,
        n_u::InvalidParameterException,n_u::IOException)
{
    ProjectConfigs configs;
    configs.parseXML(xmlFile);

    const list<const ProjectConfig*>& cfgs = configs.getConfigs();

    if (cfgs.size() == 0) throw n_u::InvalidParameterException("no configs");

    const ProjectConfig* cfg = cfgs.back();

    ProjectConfig* newcfg = new ProjectConfig(*cfg);

    configs.removeConfig(cfg);

    newcfg->setEndTime(cend);
    configs.addConfig(newcfg);

    configs.writeXML(xmlFile);
}

int main(int argc, char** argv) 
{
    ProjConfigIO proj;

    int res = proj.parseRunstring(argc,argv);
    if (res != 0) return res;

    return proj.run();
}
