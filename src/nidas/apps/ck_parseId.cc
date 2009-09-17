/*
 ********************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved


    $LastChangedBy: dongl$

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/apps/ck_parseId.cc $
 ********************************************************************
 */

#include <iostream>
#include <string>

#include <nidas/core/Variable.h>
#include <nidas/core/DOMable.h>

using namespace nidas::core;
using namespace std;

bool parseId(std::string idstr, unsigned int& val );


int main()
{
	Variable* db = new Variable();
	//DOMable* db = new DOMable();
	unsigned int id=0;
	//int id = 0;
	string idstr= "22";
	bool ret=parseId(idstr, id);
	printf("\nexpecting 22, got=%i ret=%i", id, ret);
	//assert(id==22);
	//printf("\nexpecting 22, got=%i", id);

	id=0;
	idstr= "022";
	ret=parseId(idstr, id);
	//assert(id==18);
	printf("\nexpecting 18, got=%i  ret=%i", id, ret);

	id=0;
	idstr= "0x22";
	ret=parseId(idstr, id);
	//assert(id==34);
	printf("\nexpecting 34, got=%i ret=%i", id, ret);

	return 0;
}

bool parseId(std::string idstr, unsigned int& val ) {

	istringstream ist(idstr);
	// If you unset the dec flag, then a leading '0' means
	// octal, and 0x means hex.
	ist.unsetf(std::ios::dec);

	istringstream temp(ist.str());
	char* cc= new char[2];
	temp.read(cc, 2);
	if (cc[0] == EOF) return false;

	if ( cc[0] == '0' ) {
		if (cc[1] == EOF) return false;

		if (cc[1]=='x') {
			ist>>std::hex>>val;
		} else {
			ist>>std::oct>>val;
		}
	} else ist >> val;

	if (ist.fail()) return false;

	return true;
}
