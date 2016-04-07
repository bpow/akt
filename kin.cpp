#include "akt.hpp"
#include "logs.hpp"
#include "kin.hpp"
#include "reader.hpp"
#include <bitset>

#define L 128

using namespace std;

//read pairs of samples to compute ibd sharing between
void read_pairs(ifstream &in, vector< pair<string, string> > &relpairs, map<string,int> &name_to_id )
{
	
	if(!in.is_open()){
		cout << "Failed to open file." << endl;
		exit(1);
	}
		
	string line = ""; 	
	while(getline(in,line)) // loop through the file
	{
		stringstream is(line);
		istream_iterator<string> begin(is);
		istream_iterator<string> end;
        vector<string> tokens(begin, end);
		
		if ( name_to_id.find(tokens[0]) == name_to_id.end() ) {
			cerr << tokens[0] << " not found in input" << endl; exit(1);
		} else if ( name_to_id.find(tokens[1]) == name_to_id.end() ) {
  			cerr << tokens[1] << " not found in input" << endl; exit(1);
		} 
		
		pair<string, string> tmps = make_pair(tokens[0], tokens[1]);
		relpairs.push_back(tmps);
	}
	
}

void make_pair_list(vector< pair<string, string> > &relpairs, vector<string> names){
	for(int j1=0; j1<names.size(); ++j1){
	for(int j2=j1+1; j2<names.size(); ++j2){
		relpairs.push_back( make_pair(names[j1], names[j2]) );
	}}
}

static void usage(){
  cerr << "Calculate IBD stats from a VCF" << endl;	
  cerr << "Usage:" << endl;
  cerr << "./akt kin in.bcf -T sites.vcf.gz" << endl;
  cerr << "Expects input.bcf to contain genotypes." << endl;
  cerr << "User must specify a file containing allele frequencies with -R!" << endl;
  cerr << "\t -k --minkin:			threshold for relatedness output (none)" << endl;
  cerr << "\t -u --unnorm:			If present don't normalize" << endl;
  cerr << "\t -c --calc:			calculate AF from data" << endl;
  	umessage('h');
  	umessage('T');
	umessage('n');
	umessage('a');
	umessage('r');
	umessage('R');
	umessage('s');
	umessage('S');
	umessage('f');
	umessage('m');
  exit(1);
}


int kin_main(int argc, char* argv[])
{
	

		
  int c;
  
  if(argc<3) usage();
  static struct option loptions[] =    {
    {"regions",1,0,'r'},	
    {"nthreads",1,0,'n'},	
	{"regions-file",1,0,'R'},
	{"targets-file",1,0,'T'},
    {"aftag",1,0,'a'},
    {"minkin",1,0,'k'},
    {"thin",1,0,'h'},
    {"unnorm",1,0,'u'},
    {"calc",1,0,'c'},
    {"maf",1,0,'m'},
	{"pairfile",1,0,'f'},
	{"samples",1,0,'s'},
    {"samples-file",1,0,'S'},
    {0,0,0,0}
  };

  string pfilename = "";
  bool get_regions = false; string regions = "";
  bool norm = true;
  float min_kin = 0; bool tk = false;
  int thin = 1;
  int nthreads = -1;
  float min_freq = 0;
  string pairfile="";
  string af_tag = "";
  sample_args sargs;
  bool regions_is_file = false;
  bool used_r = false;
  bool used_R = false;
  bool target = false;
  bool calc = false;
  
  while ((c = getopt_long(argc, argv, "r:R:T:k:h:n:um:f:a:s:S:c",loptions,NULL)) >= 0) {  
    switch (c)
      {
      case 'r': get_regions = true; regions = (optarg); used_r = true; break;
      case 'R': get_regions = true; regions = (optarg); pfilename = (optarg); used_R = true; regions_is_file = true; break;
      case 'T': target = true; pfilename = (optarg); break;
      case 'k': tk = true; min_kin = atof(optarg); break;
      case 'h': thin = atoi(optarg); break;
      case 'u': norm = false; break;
      case 'c': calc = true; break;
	  case 'm': min_freq = atof(optarg); break;
      case 'n': nthreads = atoi(optarg); break;
      case 'f': pairfile = (optarg); break;
      case 'a': af_tag = string(optarg) + "_"; break;
      case 's': sargs.sample_names = (optarg); sargs.subsample = true; break;
      case 'S': sargs.sample_names = (optarg); sargs.subsample = true; sargs.sample_is_file = 1; break;
      case '?': usage();
      default: cerr << "Unknown argument:"+(string)optarg+"\n" << endl; exit(1);
      }
  }

  if( used_r && used_R ){ cerr << "-r and -R cannot be used simultaneously" << endl; exit(1); }
  if( !used_R && !target ){ cerr << "Must use one of -R or -T" << endl; exit(1); }
  	
  if(nthreads < 1){ nthreads = 1; }
  omp_set_num_threads(nthreads);
  #pragma omp parallel
  {
    if(omp_get_thread_num() == 0){
      if( omp_get_num_threads() != 1){
		cerr << "Using " << omp_get_num_threads() << " threads" << endl;
		nthreads = omp_get_num_threads();
      }
    }
  }

  optind++;
  string filename = argv[optind];
			
  int Nsamples;
  vector<string> names;
  map<string,int> name_to_id;
	  
  int sites=0,markers=0,num_sites=0,num_study=0;
  
  bcf_srs_t *sr =  bcf_sr_init() ; ///htslib synced reader.
  sr->collapse = COLLAPSE_NONE;
  sr->require_index = 1;
  
  if(regions != ""){
		if ( bcf_sr_set_regions(sr, regions.c_str(), regions_is_file)<0 ){
			cerr << "Failed to read the regions: " <<  regions << endl; return 0;
		}
  }
	
  if(!(bcf_sr_add_reader (sr, filename.c_str() ))){ 
    cerr << "Problem opening " << filename << endl; 
    cerr << "Input file not found." << endl;
    bcf_sr_destroy(sr);	
    return 0;
  }
  if(!(bcf_sr_add_reader (sr, pfilename.c_str() ))){ 
	cerr << "Problem opening " << pfilename << endl; 
	cerr << "Sites file not found." << endl;
	bcf_sr_destroy(sr);	
	return 0;
  }
  
  if(sargs.subsample){ bcf_hdr_set_samples(sr->readers[0].header, sargs.sample_names, sargs.sample_is_file); }
  
  int N = bcf_hdr_nsamples(sr->readers[0].header);	///number of samples in pedigree
  cerr << N << " samples" << endl;
  Nsamples = N;
  for(int i=0; i<N; ++i){ 
	string tmp = sr->readers[0].header->samples[i]; 
	names.push_back(tmp);
	name_to_id[tmp] = i;
  }
  
  vector< vector< vector< bitset<L> > > > bits(N); //[sample][site][type][val]

  int count=0;

  bcf1_t *line, *line2;///bcf/vcf line structure.
	
  int *gt_arr=(int *)malloc(N*2*sizeof(int)),ngt=N*2,ngt_arr=N*2;
  float *af_ptr=(float *)malloc(1*sizeof(float)); int nval = 1;

  int bc = 0;
  //IBD DENOMINATOR
  float n00 = 0;
  float n10 = 0;
  float n11 = 0;
  float n20 = 0;
  float n21 = 0;
  float n22 = 0;
  
  while(bcf_sr_next_line (sr)) { 
		
    if( bcf_sr_has_line(sr,1) ){	//present in sites file.
	  line2 =  bcf_sr_get_line(sr, 1);
      if(line2->n_allele == 2){		//bi-allelic

		int nmiss=0;
		int npres=0;
		int sum = 0;
		num_sites++;
		if(bcf_sr_has_line(sr,0)) { //present in the study file
			line =  bcf_sr_get_line(sr, 0);
			ngt = bcf_get_genotypes(sr->readers[0].header, line, &gt_arr, &ngt_arr);  

			for(int i=0;i<2*N;i++){ 
				if(gt_arr[i]==bcf_gt_missing || (bcf_gt_allele(gt_arr[i])<0) || (bcf_gt_allele(gt_arr[i])>2)  ){
					gt_arr[i] = -1;
					++nmiss;
				} else {
					sum += bcf_gt_allele(gt_arr[i]);
					++npres;
				}
			}

			//only take one of every 'thin' of these sites
			if((count++)%thin==0 ){ 

				float p;
				if(calc){	///calculate AF from data
				  p = (float)sum / (float)(npres);	///allele frequency
				} else {
				  line2 =  bcf_sr_get_line(sr, 1);
				  int ret = bcf_get_info_float(sr->readers[1].header, line2, (af_tag + "AF").c_str(), &af_ptr, &nval);
				  if( nval != 1 ){ cerr << (af_tag + "AF") << " read error at " << line2->rid << ":" << line->pos+1 << endl; exit(1); }
				  p = af_ptr[0];
				}
				if( (p < 0.5) ? ( p > min_freq ) : (1-p > min_freq) ){ 	//min af
				
					float q = 1-p;
					n00 += 2*p*p*q*q;
						
					n11 += 2*p*q; //2ppq + 2qqp = 2pq(p+q) = 2pq == Hij
					n10 += 4*p*q*(p*p + q*q); //4pppq + 4qqqp = 4pq(pp + qq)
						
					n20 += p*p*p*p + q*q*q*q + 4*q*q*p*p; //pppp + qqqq + 4ppqq 
					n21 += p*p + q*q; //ppp + qqq + ppq + pqq = pp(p+q) + qq(q+p) = pp + qq
					n22 += 1; 
    
					for(int i=0; i<N; ++i){ 
						if(bc == 0){ bits[i].push_back( vector< bitset<L> >(4, bitset<L>() ) ); }
						if(gt_arr[2*i] != -1 && gt_arr[2*i+1] != -1){
							bits[i].back()[ bcf_gt_allele(gt_arr[2*i]) + bcf_gt_allele(gt_arr[2*i+1]) ][bc] = 1;
						} else {
							bits[i].back()[ 3 ][bc] = 1;
						}
						
					}
					bc = (bc+1)%(L);
			  
					++markers;
			  }
		
		} //thin
			
		++num_study;
		} //in study
      }//biallelic
      ++sites;
    }//in sites
  }//reader
  
  free(gt_arr);
  free(af_ptr);
  bcf_sr_destroy(sr);	

  cerr << "Kept " << markers << " markers out of " << sites << " in panel." << endl;
  cerr << num_study << "/"<<num_sites<<" of study markers were in the sites file"<<endl;
  
  //IBD0 IBD1 IBD2 NSNP
  vector< vector<vector<float> > > IBD( 4, vector< vector<float> >(Nsamples, vector<float>(Nsamples,0) ) );
  vector< pair<string, string> > relpairs;
  if( pairfile != "" ){
	ifstream in(pairfile.c_str());
	read_pairs(in, relpairs, name_to_id);
	in.close();
  } else {
	make_pair_list(relpairs, names);
  }

  #pragma omp parallel for	
  for(int r=0; r<relpairs.size(); ++r){ //all sample pairs

	int j1 = name_to_id[ relpairs[r].first  ]; //sample ids
	int j2 = name_to_id[ relpairs[r].second ];

	IBD[0][j1][j2] = 0;
	IBD[2][j1][j2] = 0;
	IBD[3][j1][j2] = 0;
	for(int i=0; i<bits[j1].size(); ++i){
		IBD[0][j1][j2] += (bits[j1][i][0] & bits[j2][i][2]).count() + (bits[j1][i][2] & bits[j2][i][0]).count();
		IBD[2][j1][j2] += (bits[j1][i][0] & bits[j2][i][0]).count() + (bits[j1][i][1] & bits[j2][i][1]).count() + (bits[j1][i][2] & bits[j2][i][2]).count();
		IBD[3][j1][j2] += (bits[j1][i][3] | bits[j2][i][3]).count();
	}
	IBD[1][j1][j2] = markers-IBD[3][j1][j2]-IBD[0][j1][j2]-IBD[2][j1][j2];

	IBD[0][j1][j2] /= n00;	
	IBD[1][j1][j2] = (IBD[1][j1][j2] - IBD[0][j1][j2]*n10)/n11;
	IBD[2][j1][j2] = (IBD[2][j1][j2] - IBD[0][j1][j2]*n20 - IBD[1][j1][j2]*n21)/n22;
	IBD[3][j1][j2] = n22 - IBD[3][j1][j2];

	if( norm ){
		//fit in [0,1]
		if(IBD[0][j1][j2] > 1){ //very unrelated, project to 100
		   IBD[0][j1][j2] = 1;  IBD[1][j1][j2] = 0; IBD[2][j1][j2] = 0; 
		}
		if(IBD[1][j1][j2] < 0){ IBD[1][j1][j2] = 0; }
		if(IBD[2][j1][j2] < 0){ IBD[2][j1][j2] = 0; }
		
		float sum = IBD[0][j1][j2] + IBD[1][j1][j2] + IBD[2][j1][j2];
		for(int k=0; k<3; ++k){ IBD[k][j1][j2] /= sum; }
	}
  }
	
  for(int r=0; r<relpairs.size(); ++r){ //all sample pairs

	  int i = name_to_id[ relpairs[r].first  ]; //sample ids
	  int j = name_to_id[ relpairs[r].second ];
	 
      float ks = 0.5 * IBD[2][i][j] + 0.25 * IBD[1][i][j];
      if( tk ){
		if( ks > min_kin ){
		cout << names[i] << " " << names[j] 
		   << " " << IBD[0][i][j]  << " " << IBD[1][i][j]  << " " << IBD[2][i][j] 
		   << " " << ks << " " << IBD[3][i][j] << "\n";
		}
	  } else {
		cout << names[i] << " " << names[j] 
		 << " " << IBD[0][i][j]  << " " << IBD[1][i][j]  << " " << IBD[2][i][j] 
		 << " " << ks << " " << IBD[3][i][j] << "\n";	
	  }
  }
  return 0;
}