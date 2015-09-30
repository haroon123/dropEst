#include <iostream>
#include <ostream>
#include <iterator>
#include <fstream>
#include <string>
#include <vector>
#include <climits>
#include <list>
#include <functional>
#include <algorithm>
#include <sstream>
#include <numeric>
#include <utility>
#include <set>
#include <getopt.h>
#include <unordered_map>
#include <unordered_set>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/functional/hash.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <bam.h>
#include "edit_distance.h"

using namespace std;
using namespace __gnu_cxx; 

#undef DEBUG
//#define DEBUG 1

typedef unordered_map<std::string, int,boost::hash<string> > SIHM;
typedef unordered_map<std::string, SIHM,boost::hash<string> > SHHM;
typedef unordered_map<std::string, SHHM,boost::hash<string> > SHHHM;
typedef unordered_map<std::string, SHHHM,boost::hash<string> > SHHHHM;

typedef unordered_map<int, int> IIHM;
typedef unordered_map<std::string, IIHM> SIIHM;
typedef unordered_map<int, unordered_set<int> > ISIHM;


bool firstDecSort(pair<int,int> i,pair<int,int> j) { return(i.first>j.first); }
bool firstIncSort(pair<int,int> i,pair<int,int> j) { return(i.first<j.first); }
bool secondDecSort(pair<string,int> i,pair<string,int> j) { return(i.second>j.second); }

static void usage() {
  cerr << "\tindropest: estimate molecular counts per cell"<<endl;
  cerr << "SYNOPSIS\n";
  cerr << "\tindropest [-g|--min-genes 1000] [-u|--min-umis 10000] [-m|--merge-cell-tags] [-R|--output-r] [-v|--verbose] file1.bam [file2.bam ...]"<<endl;
  cerr << "OPTIONS:\n";
  cerr << "\t-o, --output-file filename : output file name"<<endl;
  cerr << "\t-g, --min-genes n : output cells with at least n genes"<<endl;
  cerr << "\t-u, --min-umis k : output cells with at least k UMIs"<<endl;
  cerr << "\t-m, --merge-cell-tags : merge linked cell tags"<<endl;
  cerr << "\t-R, --otuput-r : write out RData file"<<endl;
}

int main(int argc,char **argv) {
  bool verbose=false;
  bool merge_tags=false;
  string oname;
  bool onameset=false;
  
  int min_genes=0;
  int min_umis=0;
  int read_prefix_length=6;

  double min_merge_fraction=0.4;
  int max_merge_edit_distance=2;
  

  int option_index=0;
  int c;
  static struct option long_options[] = {
    {"verbose", no_argument, 0, 'v'},
    {"merge-cell-tags", no_argument, 0, 'm'},
    {"output-file", required_argument, 0, 'o'},
    {"min-genes", required_argument, 0, 'g'},
    {"min-umis", required_argument, 0, 'u'},
    { 0, 0, 0, 0 }
  };
  while (( c=getopt_long(argc, argv, "vmo:g:u:",long_options, &option_index )) != -1) {
    switch(c) {
    case 'v' :
      verbose=true;
      break;
    case 'm' :
      merge_tags=true;
      break;
    case 'g' :
      min_genes = atoi( optarg );
      break;
    case 'u' :
      min_umis = atoi( optarg );
      break;
    case 'o' :
      oname = string( optarg );
      onameset=true;
      break;
    default:
      cerr<<"indropest: unknown arguments passed"<<endl;
      usage();
      return 1;
    }
  }

  if(optind > argc - 1) {
    cerr << "indropset: at least one bam file must be supplied"<<endl;
    usage();
    return 1;
  }
  
  if(!onameset) {
    oname="cell.counts.txt";
  }

  if(min_genes==0 && min_umis==0) { min_genes=1000; }

  int min_min_genes=10; // hard threshold for computational optimizationx
  if(min_genes>0 & min_genes<min_min_genes) { min_min_genes=min_genes;}

  long int readn=0; // all reads
  long int reade=0; // exonic reads
  
  int cbn=0;
  vector<SHHM> cb_genes;
  vector<string> cb_names;
  SIHM cb_ids;
  SIIHM umig_cbs;
  //SIIHM umip_cbs;

  // iterate over files
  while(optind<argc) {
    string bam_file = string(argv[optind++]);
    if(verbose) cout<<"reading "<<bam_file<<' '<<flush;
    
    bam1_t* b=bam_init1();
    bamFile in= bam_open(bam_file.c_str(), "r");

    if(in==NULL) {
      cerr<<"can't open "<<bam_file<<endl;
      return 1;
    }
    if(b==NULL) { 
      cerr<<"can't read "<<bam_file<<endl;
      return 1;
    }
    bam_header_t *header;
    header = bam_header_read(in);

    while(bam_read1(in,b) >= 0) {
      readn++;
      if(verbose && (readn % 1000000 == 0)) { 
	cout<<"."<<flush; 
      }
      const bam1_core_t *c = &b->core;
      uint8_t* ptr = bam_aux_get(b, "GE");
      if(ptr) {
	reade++;
	// parse out the tags
	string nam(bam1_qname(b));
	size_t umin=nam.rfind('#');
	if(umin==string::npos) {
	  cerr<<"WARNING: unable to parse out UMI in qname: "<<nam<<endl;
	  continue;
	}
	string umi=nam.substr(umin+1,nam.length()-umin);
	size_t ctn=nam.rfind('!',umin);
	if(ctn==string::npos) {
	  cerr<<"WARNING: unable to parse out cell tag in qname: "<<nam<<endl;
	  continue;
	}
	string ct=nam.substr(ctn+1,umin-ctn-1);
	
	
#ifdef DEBUG
	// unpack first part of the read
	char iseq[read_prefix_length];
	const bam1_core_t *c = &b->core;
	uint8_t *s = bam1_seq(b);
	if(c->l_qseq<read_prefix_length) {
	  cerr<<"WARNING: read is shorter than read_prefix_lengt. readn="<<readn<<endl;
	  continue;
	}
	for(int i = 0; i < read_prefix_length; i++) iseq[i]=bam_nt16_rev_table[bam1_seqi(s, i)];

	string prefix(iseq);
	cout<<nam<<" cell:"<<ct<<" UMI:"<<umi<<" prefix:"<<iseq;
#endif
	string gene(bam_aux2Z(ptr));
#ifdef DEBUG
	cout<<"\tXF:"<<gene<<endl;
#endif
	// insert into the cb map
	auto res=cb_ids.emplace(ct,cbn);
	if(res.second) { // new cb
	  cb_genes.push_back(SHHM());
	  cb_names.push_back(ct);
	  cbn++; 
	}
	int cb_id=res.first->second;
	cb_genes[cb_id][gene][umi]++;
	string umig=umi+gene; // +iseq
	umig_cbs[umig][cb_id]++;
	//string umip=umi+prefix; // +iseq
	//umip_cbs[umip][cb_id]++;
	
#ifdef DEBUG
	cout<<"CB/UMI="<<cb_genes[cb_id][gene][umi]<<" gene="<<cb_genes[cb_id][gene].size()<<" CB="<<cb_genes[cb_id].size()<<" UMIg="<<umig_cbs[umig].size()<<endl;
#endif	
      }
    }
    bam_header_destroy(header);
    bam_close(in);
    bam_destroy1(b);
    if(verbose) { 
      cout<<" done ("<<readn<<" total reads; "<<reade<<" exonic reads; "<<cb_genes.size()<<" cell barcodes)"<<endl;
    }
  }


  // count the number of UMIs per cb
  SIHM umi_map;
  
  vector<pair<int,int> > cb_genen;
  for(int i=0;i<cb_genes.size();i++) {
    int ngenes=cb_genes[i].size();
    if(ngenes>=min_min_genes) {
      cb_genen.push_back(pair<int,int>(ngenes,i));
    }
  }
  if(verbose) {
    cout<<cb_genen.size()<<" CBs with more than "<<min_min_genes<<" genes"<<endl;
  }
  
  std::sort(cb_genen.begin(),cb_genen.end(),firstIncSort);
  
  
  if(verbose) {
    cout<<"top CBs:"<<endl;
    for(int i=cb_genen.size()-1;i>cb_genen.size()-10;i--) {
      cout<<cb_genen[i].first<<"\t"<<cb_names[cb_genen[i].second]<<endl;
    }
  }
  
  vector<int> unmerged_cbs;
  
  if(merge_tags) {
    // cb merging 
    // reassigned barcodes ids
    vector<int> cb_reassigned(cbn);
    for(int i=0;i<cbn;i++) { cb_reassigned[i]=i; }
  
    ISIHM cb_reassigned_to;
    

    if(verbose) {
      cout<<"merging linked tags "<<flush;
    }
    int nmerges=0;
  
    int ti=0;
    for(auto c:cb_genen) {
      ti++;
      if(verbose && (ti % 1000 == 0)) { 
	cout<<"."<<flush; 
      }
      int kid=c.second;
      //cout<<endl<<"cell: "<<cb_names[kid]<<" with "<<cb_genes[kid].size()<<" genes:"<<endl;
    
      IIHM umig_top;
      int umig_n=0;
      for(auto i: cb_genes[kid]) {
	string gene=i.first;
	//cout<<gene<<":[";
	for(auto j: i.second) {
	  //cout<<j.first<<":(";
	  //look up umig
	  string umig=j.first+gene;
	  for(auto k: umig_cbs[umig]) {
	    //cout<<k.first<<" ";
	    if(k.first!=kid) {
	      umig_top[cb_reassigned[k.first]]++;
	    }
	  }
	  umig_n++;
	  //cout<<") ";
	}
	//cout<<"]"<<endl;
      }
      // get top umig, its edit distance
      int top_cb=-1; double top_cb_f=-1; int top_cb_ngenes=-1;
      for(auto l: umig_top) {
	double cb_f=(((double)l.second)/((double)umig_n));
	if(cb_f>top_cb_f || (cb_f==top_cb_f && cb_genes[l.first].size() > top_cb_ngenes)) { 
	  top_cb=l.first; top_cb_f=cb_f; top_cb_ngenes=cb_genes[l.first].size();
	}
      }
      bool merged=false;
      if(top_cb>0) {
	//cout<<"\ttop cb: "<<cb_names[top_cb]<<" ("<<top_cb_ngenes<<" genes)="<<top_cb_f<<" ";
	// check if the top candidate is valid for merging
	if(top_cb_f>min_merge_fraction) {
	  int ed=edit_distance(cb_names[top_cb].c_str(),cb_names[kid].c_str());
	  //cout<<" ed="<<ed<<endl;
	  if(ed<max_merge_edit_distance) {
	    // do the merge
	    merged=true;
	    //cout<<"merging "<<kid<<" ("<<cb_genes[kid].size()<<" genes) into "<<top_cb<<" ("<<top_cb_ngenes<<" genes) ";
	    nmerges++;
	
	    // merge the actual data
	    for(auto l: cb_genes[kid]) {
	      for(auto m: l.second) {
		cb_genes[top_cb][l.first][m.first]+=m.second;
	      }
	    }
	    // establish new mapping
	    cb_reassigned[kid]=top_cb; // set reassignment mapping
	    cb_reassigned_to[top_cb].insert(kid); // reassign current cell
	  
	    // transfer mapping of the cbs previously mapped to kid
	    auto k=cb_reassigned_to.find(kid);
	    if(k!=cb_reassigned_to.end()) {
	      for(auto m: k->second) {
		cb_reassigned[m]=top_cb; // update reassignment mapping
	      }
	      cb_reassigned_to[top_cb].insert(k->first); // reassign to the new cell
	    }
	    //cout<<cb_genes[top_cb].size()<<" genes"<<endl;
	    //cout<<"\t"<<top_cb<<": "<<cb_genes[top_cb].size()<<" genes, "<<cb_reassigned_to[top_cb].size()<<" reassigned cbs"<<endl;
	  }
	}
      }
      if(!merged) {
	//cout<<" not merging"<<endl;
	if(c.first>=min_genes) { // only record cells that are passing min_genes threshold
	  unmerged_cbs.push_back(kid);
	}
      }
    }
    reverse(unmerged_cbs.begin(),unmerged_cbs.end());
    cout<<" done ("<<nmerges<<" merges performed)"<<endl;

    if(verbose) {
      cout<<"top CBs:"<<endl;
      for(int i=0;i<10;i++) {
	cout<<cb_genes[unmerged_cbs[i]].size()<<"\t"<<cb_names[unmerged_cbs[i]]<<endl;
      }
    }
  
  } else {
    // just pick out all sufficiently informative cells
    for(int i=cb_genen.size()-1;i>=0;i--) {
      if(cb_genen[i].first>=min_genes) { // only record cells that are passing min_genes threshold
	unmerged_cbs.push_back(i);
      } else {
	break;
      }
    }
  }

  if(verbose) { 
    cout<<unmerged_cbs.size()<<" valid (with >="<<min_genes<<" genes) cells with ";
  }

  SIHM gene_counts;
  for(auto cb: unmerged_cbs) {
    for(auto gm: cb_genes[cb]) {
      gene_counts[gm.first]+=gm.second.size();
    }
  }
  if(verbose) { cout<<gene_counts.size()<<" genes"<<endl; }
  
  vector<pair<string, int>> t1(gene_counts.begin(), gene_counts.end());
  sort(t1.begin(), t1.end(),secondDecSort);
  
  if(verbose) {
    cout<<"top genes:"<<endl;
    for(int i=0;i<10;i++) {
      cout<<t1[i].first<<'\t'<<t1[i].second<<endl;
    }
  }

  // output UMI table
  if(verbose) { cout<<"writing output matrix to "<<oname<<" "<<flush; }
  //ofstream ofs(oname.c_str(), ios_base::out | ios_base::binary);
  //boost::iostreams::filtering_ostream ofile;
  //ofile.push(boost::iostreams::gzip_compressor());
  //ofile.push(ofs);

  ofstream ofile(oname.c_str(), ios_base::out);
  // header
  ofile<<"gene";
  for(auto i: unmerged_cbs) { ofile<<'\t'<<cb_names[i]; }
  ofile<<endl;
  for(auto i: t1) { 
    ofile<<i.first; 
    for(auto cb: unmerged_cbs) { ofile<<'\t'<<cb_genes[cb][i.first].size(); }
    ofile<<endl;
  }
  
  //ofile.pop();
  //ofs.close();
  ofile.close();
  if(verbose) { cout<<" all done"<<endl; }

}