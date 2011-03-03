/*
 * sliceNidasData.cpp
 *
 *  Created on: Mar 3, 2011
 *      Author: dongl
 */

#include "sliceNidasData.h"


sliceNidasData::sliceNidasData() {}

sliceNidasData::~sliceNidasData() {}

/**
 * t1 and t2 are beg and end times in miliseconds
 */
void sliceNidasData::assignTms(unsigned long  t1, unsigned long t2) {
	n_u::UTime tt1(((long long )t1) * USECS_PER_SEC);
	n_u::UTime tt2(((long long )t2) * USECS_PER_SEC);
	_tms.push_back(tt1);
	_tms.push_back(tt2);
	cout<<"t1="<<tt1.format(true, "%Y %m %d %H:%M:%S")<<endl;
	cout<<"t2="<<tt2.format(true, "%Y %m %d %H:%M:%S")<<endl;
}

/**
 * tm format is: yyyy-mm-dd hh:mm:ss
 */
void sliceNidasData::assignTms(string t1, string t2) {

	int y, m, d, h, min, s;
	int yy, mm, dd, hh, mmin, ss;
	y = atoi(t1.substr(0,4).c_str());
	m = atoi(t1.substr(5,2).c_str());
	d = atoi(t1.substr(8,2).c_str());
	h = atoi(t1.substr(11,2).c_str());
	min = atoi(t1.substr(14,2).c_str());
	s = atoi(t1.substr(17,2).c_str());

	yy = atoi(t2.substr(0,4).c_str());
	mm = atoi(t2.substr(5,2).c_str());
	dd = atoi(t2.substr(8,2).c_str());
	hh = atoi(t2.substr(11,2).c_str());
	mmin = atoi(t2.substr(14,2).c_str());
	ss = atoi(t2.substr(17,2).c_str());

	cout<<" y="<<y<<" m="<<m<<" d="<<d<<" h="<<h<<" min="<<min<<" s="<<s<<endl;
	cout<<" yy="<<yy<<" mm="<<mm<<" dd="<<dd<<" hh="<<hh<<" mmin="<<mmin<<" s="<<s<<endl;

	n_u::UTime tt1(true, y,m,d,h,min,s,0);
	n_u::UTime tt2(true, yy,mm,dd,hh,mmin,ss,0);
	_tms.push_back(tt1);
	_tms.push_back(tt2);
	cout<<"t1="<<tt1.format("%c");
	cout<<"t2="<<tt2.format("%c");
}



void  sliceNidasData::help(){
	cout<<"slice-nidas-data paramters:"<<endl;
	cout<<" -i required: input-nidas-data-file-name"<<endl;
	cout<<" -o required: output-file-name"<<endl;
	cout<<" -b beg-time"<<"either utime-seconds or \"yyyy-mm-dd hh:mm:ss\""<<endl;
	cout<<" -e end-time"<<"either utime-seconds or \"yyyy-mm-dd hh:mm:ss\""<<endl;
}

int sliceNidasData::parseRunstring(int argc, char** argv){
	if (argc<1) {
		WLOG(("No file input"));
		return -1;
	}
	extern char *optarg;       /* set by getopt() */
	int opt_char;     /* option character */
	//	extern int optind;       /* "  "     "     */

	string t1, t2;
	while ((opt_char = getopt(argc, argv, "i:o:b:e:x")) != -1) {
		switch (opt_char) {
		case 'i':
			_inputFileNames.push_back(optarg);
			cout<< "infile="<< optarg<<endl;
			break;
		case 'o':
			_outputFileName = optarg;
			break;
		case 'b':
			t1 = optarg;
			break;
		case 'e':
			t2 = optarg;
			break;
		case 'x':
			//_fileoutdir = optarg;
			break;
		}

	}
	if (_inputFileNames.empty() || _outputFileName.empty() || t1.empty() || t2.empty()) {
		help();
		return -1;
	}

	cout<<"parserarg-t1="<<t1<<endl;
	cout<<"parserarg-t2="<<t2<<endl;
	cout<<"parserarg-outfile="<<_outputFileName<<endl;

	if (t1.find("_")!=string::npos || t1.find(":")!=string::npos){
		cout<<"tm-goes-tostring"<<endl;
		assignTms(t1,t2);
	}
	else {
		cout<<"tm-goes-long"<<endl;
		assignTms(strToInt(t1), strToInt(t2));
	}

	return 1;
}
void sliceNidasData::setInputFile(list<string> files){
	_inputFileNames.merge(files);
}
void sliceNidasData::setInputFile(string filein){
	_inputFileNames.push_back(filein);
}
void sliceNidasData::setOutputFile(string fileout){
	_outputFileName = fileout;
}

int sliceNidasData::run()
{

	try {
		nidas::core::FileSet* outSet = 0;
		if (_outputFileName.find(".bz2") != string::npos) {
		#ifdef HAS_BZLIB_H
			outSet = new nidas::core::Bzip2FileSet();
		#else
			cerr << "Sorry, no support for Bzip2 files on this system" << endl;
			exit(1);
		#endif
		} else
			outSet = new nidas::core::FileSet();

		outSet->setFileName(_outputFileName);
		outSet->setFileLengthSecs(86400); //one day

		SampleOutputStream outStream(outSet);
		outStream.setHeaderSource(this);
		IOChannel* iochan = nidas::core::FileSet::getFileSet(_inputFileNames);

		// RawSampleInputStream owns the iochan ptr.
		RawSampleInputStream input(iochan);
		input.setMaxSampleLength(32768);
		input.readInputHeader();

		// save header for later writing to output
		_header = input.getInputHeader();

		try {
			for (;;) {
				Sample* samp = input.readSample();
				if (samp->getTimeTag() < _tms[0].toUsecs() || samp->getTimeTag() > _tms[1].toUsecs()) continue;
				outStream.receive(samp);
				//for debug
				cout<<" dsm-id="<<samp->getDSMId()<<" sampleid="<<samp->getSpSId()<<endl;
				//n_u::UTime ut(samp->getTimeTag());
				//const char* data = (const char *)samp->getConstVoidDataPtr();
				//cout<<"sample= "+ut.format("%c")<<endl;
				//cout<<" data-len="<<samp->getDataByteLength()<<endl;
				//cout<<" data="<<data<<endl;

				samp->freeReference();
			}
		}
		catch (n_u::EOFException& ioe) {
			cerr << ioe.what() << endl;
		}

		outStream.finish();
		outStream.close();
	}
	catch (n_u::IOException& ioe) {
		cerr << ioe.what() << endl;
		return 1;
	}
	return 0;
}

void sliceNidasData::sendHeader(dsm_time_t thead,SampleOutput* out) throw(n_u::IOException)
{
	//printHeader();
	_header.write(out);
}


unsigned int sliceNidasData::strToInt(string str){
	unsigned int a;
	stringstream stream;
	stream<<str<<flush;
	stream>>a;
	return a;
}

/**
 * this program can take several parameters
 *  -i: one input data file name // us -i more each input file //can be more than one input files
 *  -o: the output file name
 *  -b: beg-time when the the data is abstracted
 *  -e: end-time when data stops
 */
int main (int argc, char **argv) {
	sliceNidasData snd;
	if (snd.parseRunstring(argc, argv) < 0) {
		return -1;
	}
	cout<<endl<<"start to slice nidas data "<<endl;
	try {
		snd.run();
	} catch (n_u::Exception e){cerr<<e.toString()<<endl;}
	return 1;
}

