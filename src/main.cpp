#include <stdio.h>
#include <ctime>
#include <fstream>
#include <string>

#include "graph_align.hpp"
#include "graph_builder.hpp"
#include "graph_io.hpp"
#include "graph_kmerify.hpp"
#include "graph.hpp"
#include "constants.hpp"

#include <seqan/arg_parse.h>
#include <seqan/bam_io.h>

#include <boost/algorithm/string/find.hpp>


template<typename T>
void parseArgList (CharString &args_in, std::vector<T> &vector_out)
{
  unsigned j = 0;
  for (unsigned i = 0 ; i < length(args_in) ; ++i)
  {
    if (args_in[i] != ',')
      continue;

    CharString copy(args_in);
    copy = infix(copy, j, i);
    std::string new_beta(toCString(copy));
    T new_beta_casted = boost::lexical_cast<T>(new_beta);
    j = i+1;
    vector_out.push_back(new_beta_casted);
  }

  CharString copy(args_in);
  copy = suffix(copy, j);
  std::string new_beta(toCString(copy));
  T new_beta_casted = boost::lexical_cast<T>(new_beta);
  vector_out.push_back(new_beta_casted);
}


ArgumentParser::ParseResult parseCommandLine(callOptions & CO, ArgumentParser & parser, int argc, char const ** argv )
{
  // setShortDescription(parser, "Converts a bam file into a fastq file.");
  setVersion(parser, "0.1");
  setDate(parser, "July 2015");

  // Main options
  addSection(parser, "Main options");
  addOption(parser, ArgParseOption("o", "output", "Outfile folder location ", ArgParseArgument::STRING, "output"));
  addOption(parser, ArgParseOption("f", "vcf_output", "Outfile folder location ", ArgParseArgument::STRING, "vcf_output"));
  addOption(parser, ArgParseOption("ms", "minSeqLen", "Minimum sequence length ", ArgParseArgument::STRING, "STRING"));
  addOption(parser, ArgParseOption("qc", "bpQclip", "Quality Clip Threshold for clipping. ", ArgParseArgument::INTEGER, "INT"));
  addOption(parser, ArgParseOption("qs", "bpQskip", "Quality Clip Threshold for skipping. ", ArgParseArgument::INTEGER, "INT"));
  addOption(parser, ArgParseOption("k", "kmer", "The length of the k-mers.", ArgParseArgument::INTEGER, "INT"));
  addOption(parser, ArgParseOption("mk", "min_kmers", "The minimum amount of k-mers.", ArgParseArgument::INTEGER, "INT"));
  addOption(parser, ArgParseOption("v", "verbose", "Verbosity flag"));
  addOption(parser, ArgParseOption("23", "exon_2_and_3", "Only use graph with exons 2 and 3."));
  addOption(parser, ArgParseOption("c", "vcf", "Print VCF outut."));
  addOption(parser, ArgParseOption("bc", "bias_check", "If used Gyper will use every read pair in the BAM file provided. Should only be used for very small BAMs, unless you have huge amount of RAM and patience."));
  addOption(parser, ArgParseOption("1k", "thousand_genomes", "Use reference from the 1000 Genomes project."));
  addOption(parser, ArgParseOption("b",
    "beta",
    "A scoring parameter for heterozygous scores. Higher beta means more likely to pick heterozugous.",
    ArgParseArgument::STRING, "STRING"));

  setDefaultValue(parser, "beta", "0.6");
  addOption(parser, ArgParseOption("b2", "bam2", "Second bam file", ArgParseArgument::INPUT_FILE, "bam2"));
  addOption(parser, ArgParseOption("b3", "bam3", "Third bam file", ArgParseArgument::INPUT_FILE, "bam3"));
  addOption(parser, ArgParseOption("b4", "bam4", "Fourth bam file", ArgParseArgument::INPUT_FILE, "bam4"));
  addOption(parser, ArgParseOption("rg", "read_gap", "The amount af bp to extend the read gap (i.e. the gap where reads are considered.) ", ArgParseArgument::INTEGER, "INT"));
  
  // setDefaultValue( parser, "qual", CO.bpQclip );

  ArgumentParser::ParseResult res = parse(parser, argc, argv);

  // Only extract  options if the program will continue after parseCommandLine()
  if (res != ArgumentParser::PARSE_OK)
    return res;

  if (isSet(parser, "minSeqLen"))
    getOptionValue(CO.minSeqLen_list, parser, "minSeqLen");
  if (isSet(parser, "output" ))
    getOptionValue(CO.outputFolder, parser, "output");
  if (isSet(parser, "vcf_output" ))
    getOptionValue(CO.vcfOutputFolder, parser, "vcf_output");
  if (isSet(parser, "bpQclip" ))
    getOptionValue(CO.bpQclip, parser, "bpQclip");
  if (isSet(parser, "bpQskip" ))
    getOptionValue(CO.bpQskip, parser, "bpQskip");
  if (isSet(parser, "kmer" ))
    getOptionValue(CO.kmer, parser, "kmer");
  if (isSet(parser, "min_kmers" ))
    getOptionValue(CO.min_kmers, parser, "min_kmers");
  if (isSet(parser, "bam2" ))
    getOptionValue(CO.bam2, parser, "bam2");
  if (isSet(parser, "bam3" ))
    getOptionValue(CO.bam3, parser, "bam3");
  if (isSet(parser, "bam4" ))
    getOptionValue(CO.bam4, parser, "bam4");
  if (isSet(parser, "beta"))
    getOptionValue(CO.beta_list, parser, "beta");
  if (isSet(parser, "bias_check"))
    getOptionValue(CO.bias_check, parser, "bias_check");
  if (isSet(parser, "thousand_genomes"))
    getOptionValue(CO.thousand_genomes, parser, "thousand_genomes");
  if (isSet(parser, "read_gap"))
    getOptionValue(CO.read_gap, parser, "read_gap");

  // Check if multiple values very given in a list
  parseArgList(CO.beta_list, CO.beta);
  parseArgList(CO.minSeqLen_list, CO.minSeqLen);
  getArgumentValue( CO.gene, parser, 0);
  getArgumentValue( CO.bamFile, parser, 1);
  CO.verbose = isSet(parser, "verbose");
  CO.exon_2_and_3 = isSet(parser, "exon_2_and_3");
  CO.vcf = isSet(parser, "vcf");

  // Remove the HLA- part from genes if it is there
  if (CO.gene == "HLA-A")
  {
    CO.gene = "HLAA";
  }
  else if (CO.gene == "HLA-B")
  {
    CO.gene = "HLAB";
  }
  else if (CO.gene == "HLA-C")
  {
    CO.gene = "HLAC";
  }
  else if (CO.gene == "HLA-DQA1")
  {
    CO.gene = "DQA1";
  }
  else if (CO.gene == "HLA-DQB1")
  {
    CO.gene = "DQB1";
  }
  else if (CO.gene == "HLA-DRB1")
  {
    CO.gene = "DRB1";
  }

  if (CO.verbose)
  {
    CO.print_options();
  }

  // Set the number of exons
  if (CO.gene == "HLAA")
  {
    CO.number_of_exons = 8;
  }
  else if (CO.gene == "HLAB")
  {
    CO.number_of_exons = 7;
  }
  else if (CO.gene == "HLAC")
  {
    CO.number_of_exons = 8;
  }
  else if (CO.gene == "DQA1")
  {
    CO.number_of_exons = 4;
  }
  else if (CO.gene == "DQB1")
  {
    CO.number_of_exons = 6;
  }
  else if (CO.gene == "DRB1")
  {
    CO.number_of_exons = 6;
  }

  return ArgumentParser::PARSE_OK;
}


typedef FormattedFileContext<BamFileIn, void>::Type TBamContext;


unsigned
getChromosomeRid(TBamContext const & bamContext, CharString const & chromosomeName)
{
  for (unsigned i = 0; i < length(contigNames(bamContext)); ++i)
  {
    if (contigNames(bamContext)[i] == chromosomeName)
    {
      return i;
    }
  }

  return 99999;
}


std::string
getRegion(callOptions & CO,
          std::string gene)
{
  std::ostringstream region_file;
  region_file << gyper_SOURCE_DIRECTORY;
  
  if (CO.thousand_genomes)
  {
    region_file << "/data/haplotypes/hla/region1k/" << gene;
  }
  else
  {
    region_file << "/data/haplotypes/hla/region/" << gene;
  }
  
  std::ifstream myfile(region_file.str());
  std::string line;

  if (myfile.is_open())
  {
    while (getline(myfile,line))
    {
      return line;
    }
    myfile.close();
  }
  std::cerr << "ERROR: No region file found at location: " << region_file.str() << std::endl;
  return line;
}


bool
validateRecord(callOptions & CO,
               TBamContext const & bamContext,
               BamAlignmentRecord const & record,
               std::string region
              )
{
  std::string delimiter = " ";
  size_t pos = 0;
  std::string token;
  while ((pos = region.find(delimiter)) != std::string::npos)
  {
    token = region.substr(0, pos);

    size_t colon_pos = token.find(":");
    std::string chromosomeName = token.substr(0, colon_pos);
    unsigned rID = getChromosomeRid(bamContext, chromosomeName);
    if (rID == 99999)
    {
      // This means we didn't find the chromosome
      rID = getChromosomeRid(bamContext, chromosomeName.substr(3));
      if (rID == 99999)
      {
        std::cerr << "I can't find the rID!" << std::endl;
      }
    }
    // std::cout << "chromosomeName = " << chromosomeName << " rID = " << rID << " record.rID = " << record.rID << std::endl;
    if (rID == (unsigned) record.rID)
    {
      size_t hyphen_pos = token.find("-");
      std::string begin_pos = token.substr(colon_pos + 1, hyphen_pos - (colon_pos + 1));
      std::string end_pos = token.substr(hyphen_pos + 1);
      if ((unsigned) record.beginPos > stoul(begin_pos) - length(record.seq) - CO.read_gap && (unsigned) record.beginPos < stoul(end_pos) + CO.read_gap)
      {
        return true;
      }
      // else
      // {
      //   std::cout << "NOPE: record.beginPos = " << record.beginPos << " begin_pos = " << begin_pos << " end_pos = " << end_pos << std::endl;
      // }
    }

    // std::cout << token << std::endl;
    region.erase(0, pos + delimiter.length());
  }
  // std::cout << "NOPE: record.rID = " << record.rID << " record.beginPos = " << record.beginPos << std::endl;
  return false;
}


bool
jumpAround(callOptions & CO,
           BamFileIn & bamFileIn,
           TBamContext const & bamContext,
           BamIndex<Bai> const & index,
           std::string & region,
           unsigned & begin_pos,
           unsigned & end_pos
          )
{
  if (region.size() < 6)
  {
    // std::cerr << "Region too small: " << region << std::endl;
    return false;
  }

  std::string delimiter = " ";
  size_t pos = region.find(delimiter);
  std::string token;
  token = region.substr(0, pos);
  // std::cout << "token = " << token << std::endl;

  size_t colon_pos = token.find(":");
  std::string chromosomeName = token.substr(0, colon_pos);
  unsigned rID = getChromosomeRid(bamContext, chromosomeName);
  
  if (rID == 99999)
  {
    // This means we didn't find the chromosome as chrX, so let's try if we can find just X
    rID = getChromosomeRid(bamContext, chromosomeName.substr(3));
    if (rID == 99999)
    {
      std::cerr << "I can't find the rID!" << std::endl;
      return false;
    }
  }

  size_t hyphen_pos = token.find("-");
  std::string begin_str = token.substr(colon_pos + 1, hyphen_pos - (colon_pos + 1));
  // std::cout << "begin_str = " << begin_str << std::endl;

  begin_pos = stoul(begin_str);
  std::string end_str = token.substr(hyphen_pos + 1);
  // std::cout << "end_str = " << end_str << std::endl;
  end_pos = stoul(end_str);
  region.erase(0, pos + delimiter.length());

  bool hasAlignments;

  if (!jumpToRegion(bamFileIn, hasAlignments, rID, begin_pos, end_pos, index))
  {
    std::cerr << "Couldn't jump in bam file!" << std::endl;
    return false;
  }
  else if (CO.verbose)
  {
    std::cout << "Jumping to positions " << chromosomeName << ":" << begin_pos << "-" << end_pos << " in the bam file." << std::endl;
  }


  if (!hasAlignments)
  {
    std::cerr << "No alignments found at that location!" << std::endl;
    begin_pos = 0;
    end_pos = 0;
  }

  return true;
}

typedef boost::unordered_map< String<char>, seqan::BamAlignmentRecord > TBarMap;


int
addToBars(callOptions & CO,
          TBarMap & bars1,
          TBarMap & bars2,
          CharString & bam,
          std::string region
         )
{
  if (bam == "")
    return 0;

  if (CO.verbose)
  {
    std::cout << "Adding " << bam << std::endl;
  }
  
  std::string region_copy(region);
  char* bam_file_name = toCString(bam);
  BamFileIn bamFileIn(bam_file_name);
  BamHeader header;
  readHeader(header, bamFileIn);
  
  TBamContext const & bamContext = context(bamFileIn);
  // std::cout << "Chromosome 6 has rID: " << getChromosomeRid(bamContext, "chr6") << std::endl;
  
  BamAlignmentRecord record;

  if (!CO.bias_check)
  {
    BamIndex<Bai> baiIndex;
    if (!open(baiIndex, strcat(bam_file_name, ".bai")))
    {
      std::cerr << "ERROR: Could not read BAI index file " << bam_file_name << "\n";
      return 1;
    }

    while (true)
    {
      unsigned begin_pos;
      unsigned end_pos;

      if(!jumpAround(CO, bamFileIn, bamContext, baiIndex, region, begin_pos, end_pos))
      {
        break;
      }
      
      readRecord(record, bamFileIn);

      while((unsigned) record.beginPos < begin_pos - CO.read_gap - length(record.seq))
      {
        readRecord(record, bamFileIn);
      }

      while((unsigned) record.beginPos < end_pos + CO.read_gap && !atEnd(bamFileIn))
      {
        // std::cout << "record.beginPos = " << record.beginPos << std::endl;
        if(!validateRecord(CO, bamContext, record, region_copy))
        {
          readRecord(record, bamFileIn);
          continue;
        }

        if (hasFlagFirst(record))
        {
          if (bars1.count(record.qName) == 1)
          {
            readRecord(record, bamFileIn);
            continue;
          }

          bars1[record.qName] = record;
        }
        else
        {
          if (bars2.count(record.qName) == 1)
          {
            readRecord(record, bamFileIn);
            continue;
          }

          bars2[record.qName] = record;
        }

        readRecord(record, bamFileIn);
      }
    }
  }
  else
  {
    while (!atEnd(bamFileIn))
    {
      readRecord(record, bamFileIn);

      // std::cout << "record.beginPos = " << record.beginPos;
      // std::cout << ": record.rID = " << record.rID << std::endl;

      if (hasFlagFirst(record))
      {
        if (bars1.count(record.qName) == 1)
        {
          continue;
        }

        bars1[record.qName] = record;
      }
      else
      {
        if (bars2.count(record.qName) == 1)
        {
          continue;
        }

        bars2[record.qName] = record;
      }
    }
  }
  
  return 0;
}


inline int
qualToInt( char c )
{
  return (int) c - 33;
}


void
trimReadEnds (DnaString & seq,
              CharString & qual,
              int const & bpQclip
             )
{
  using namespace seqan;

  int beg, end;

  for (beg = 0; (beg < (int) length(seq)) && (qualToInt(qual[beg]) < bpQclip); ++beg);
  for (end = (int) length(seq) - 1; (end > 0) && (qualToInt(qual[end]) < bpQclip); --end);
  
  if (beg > end)
  {
    seq = "";
    qual = "";
  }
  else
  {
    seq = infix(seq, beg, end+1);
    qual = infix(qual, beg, end+1);
  }

  // boost::dynamic_bitset<> quality_bitset(length(seq));

  // for (unsigned pos = 0 ; pos < length(seq) ; ++pos)
  // {
  //   if (qualToInt(qual[pos]) < bpQskip)
  //   {
  //     quality_bitset[pos] = 1;
  //   }
  // }
  // return quality_bitset;
}


void
appendToFile (std::stringstream& ss, CharString &fileName)
{  
  using namespace std;
  string myString = ss.str();  
  ofstream myfile;  
  myfile.open(toCString(fileName), ios_base::app); 
  myfile << myString;  
  myfile.close();  
}


void
writeToFile (std::stringstream& ss, CharString &fileName)
{
  using namespace std;
  string myString = ss.str();
  ofstream myfile;
  myfile.open(toCString(fileName), ios_base::trunc); 
  myfile << myString;
  myfile.close();
}


std::string
fourDigit(std::string my_id)
{
  size_t n = std::count(my_id.begin(), my_id.end(), ':');

  unsigned colons;
  // if (CO.gene == "HLAB" || CO.gene == "HLAC")
  // {
  //   // Only 2 digit resolution for HLAB and HLAC
  //   colons = 0;
  // }
  // else
  // {
  //   colons = 1;
  // }
  colons = 1;

  if (n > colons)
  {
    boost::iterator_range<std::string::iterator> r = boost::find_nth(my_id, ":", colons);
    size_t distance = std::distance(my_id.begin(), r.begin());
    my_id = my_id.substr(0, distance);
  }

  char last_char = my_id.back();

  if (!isdigit(last_char))
  {
    std::string no_char_id(my_id.begin(), my_id.end() - 1);
    return no_char_id;
  }

  return my_id;
}


size_t
findAvailableGenotypes(callOptions & CO, boost::unordered_set<std::string> & available_alleles)
{
  std::ostringstream file_location;
  file_location << gyper_SOURCE_DIRECTORY;

  if (CO.thousand_genomes)
  {
    file_location << "/data/haplotypes/hla/available_alleles/1000genomes/";
  }
  else
  {
    file_location << "/data/haplotypes/hla/available_alleles/icelandic_alleles/";
  }

  file_location << CO.gene;
  std::ifstream available_genotypes_file_stream(file_location.str());
  std::string line;

  available_genotypes_file_stream >> line;
  size_t n = std::count(line.begin(), line.end(), ':');
  available_alleles.insert(line);

  while (available_genotypes_file_stream >> line)
  {
    available_alleles.insert(line);
  }

  available_genotypes_file_stream.close();
  return n;
}


void
handleOutput (callOptions & CO,
              const double & beta,
              const std::string & pn,
              std::vector<std::string> & ids,
              std::vector<std::vector<double> > seq_scores,
              double const & total_matches
             )
{
  // Four digit vector
  std::vector<std::string> ids_four;

  // The 4 digit index numbers
  boost::unordered_map<std::string, unsigned> four;
  unsigned j = 0;
  // bool no_available_alleles = false;

  for (unsigned i = 0; i < ids.size(); ++i)
  {
    // std::cout << "Id before: " << my_id << std::endl;
    std::string my_id = fourDigit(ids[i]);
    // std::cout << "Id after: " << my_id << std::endl;

    if (!four.count(my_id))
    {
      four[my_id] = j;
      ids_four.push_back(my_id);
      ++j;
    }
  }

  // if (ids_four.size() == 0)
  // {
  //   no_available_alleles = true;
  //   ids_four = ids;
    
  //   for (unsigned i = 0; i < ids.size(); ++i)
  //   {
  //     std::string my_id = ids[i];

  //     if (!four.count(my_id))
  //     {
  //       four[my_id] = j;
  //       ids_four.push_back(my_id);
  //       ++j;
  //     }
  //   }
  // }

  std::vector<std::vector<double> > seq_scores_four;
  boost::unordered_map<std::pair<unsigned, unsigned>, std::pair<unsigned, unsigned> > index_map;

  {
    std::vector<double> inner_seq_scores;
    inner_seq_scores.resize(four.size());
    std::vector<std::vector<double> > outer_seq_scores;
    seq_scores_four.resize(four.size(), inner_seq_scores);
  }

  boost::unordered_set<std::string> available_alleles;
  unsigned available_digits = findAvailableGenotypes(CO, available_alleles);
  // std::cout << "available_digits = " << available_digits << std::endl;

  std::string short_id_i;
  std::string short_id_j;

  for (unsigned i = 0 ; i < ids.size() ; ++i)
  {
    for (unsigned j = 0 ; j <= i ; ++j)
    {
      // No need to scale the homozygous solutions
      if (i != j)
      {
        seq_scores[i][j] *= beta;
        seq_scores[i][j] += (seq_scores[i][i]+seq_scores[j][j])/2.0*(1.0-beta);
      }

      {
        // Get the short ID for i and skip this iteration if it isn't available
        boost::iterator_range<std::string::iterator> r = boost::find_nth(ids[i], ":", available_digits);
        size_t distance = std::distance(ids[i].begin(), r.begin());
        short_id_i = ids[i].substr(0, distance);
        // std::cout << "short_id_i = " << short_id_i << std::endl;
      }

      if (available_alleles.find(short_id_i) == available_alleles.end())
      {
        continue;
      }

      {
        // Same for j
        boost::iterator_range<std::string::iterator> r = boost::find_nth(ids[j], ":", available_digits);
        size_t distance = std::distance(ids[j].begin(), r.begin());
        short_id_j = ids[j].substr(0, distance);
        // std::cout << "short_id_j = " << short_id_j << std::endl;
      }

      if (available_alleles.find(short_id_j) == available_alleles.end())
      {
        continue;
      }

      unsigned i_four;
      unsigned j_four;

      // if (no_available_alleles)
      // {
      //   i_four = i;
      //   j_four = j;
      // }
      // else
      // {
        i_four = four[fourDigit(ids[i])];
        j_four = four[fourDigit(ids[j])];
      // }

      if (seq_scores[i][j] > seq_scores_four[i_four][j_four])
      {
        seq_scores_four[i_four][j_four] = seq_scores[i][j];
        std::pair<unsigned, unsigned> index_pair(i, j);
        std::pair<unsigned, unsigned> index_pair_four(i_four, j_four);
        // std::cout << "Creating a index pair with: " << ids[i] << ", " << ids[j] << std::endl;
        index_map[index_pair_four] = index_pair;
      }
    }
  }

  unsigned i_max = 0;
  unsigned j_max = 0;
  double max_score = 0;
  
  for (unsigned i = 0 ; i < ids.size() ; ++i)
  {
    // std::cout << std::setw(17) << ids[i] << ": ";
    for (unsigned j = 0 ; j < ids.size() ; ++j)
    {
      // printf("%7.2f ", seq_scores[i][j]);
      
      if (seq_scores[i][j] > max_score)
      {
        i_max = i;
        j_max = j;
        max_score = seq_scores[i][j];
      }
    }
    // std::cout << std::endl;
  }
  
  unsigned i_max_short = 0;
  unsigned j_max_short = 0;
  double max_score_short = 0;

  if (CO.bias_check)
  {
    for (unsigned i = 0; i < ids.size(); ++i)
    {
      std::cout << std::setw(23) << ids[i] << " " << seq_scores[i][i] << std::endl;
    }
  }
  else
  {
    for (unsigned i = 0; i < ids_four.size(); ++i)
    {
      // Skip alleles with a score of 0.0
      if (seq_scores_four[i][0] < 0.1)
        continue;

      if (CO.verbose)
      {
        std::cout << std::setw(15) << ids_four[i] << ": ";
      }
      
      for (unsigned j = 0; j < ids_four.size(); ++j)
      {
        if (seq_scores_four[i][j] < 0.1)
          continue;

        if (i >= j && CO.verbose)
          printf("%6.1f ", seq_scores_four[i][j]);

        if (seq_scores_four[i][j] > max_score_short)
        {
          i_max_short = i;
          j_max_short = j;
          max_score_short = seq_scores_four[i][j];
        }
      }

      if (CO.verbose)
      {
        std::cout << std::endl;
      }
    }

    // Get the original alleles
    std::pair<unsigned, unsigned> best_alleles(i_max_short, j_max_short);
    std::pair<unsigned, unsigned> allele_ids = index_map[best_alleles];

    if (CO.verbose)
    {
      std::cout << "Total matches are: " << total_matches << std::endl;
    }

    if (total_matches != 0)
    {
      if (CO.verbose)
      {
        std::cout << "Oracle: Out of all the alleles, I'd say this person has   " << ids[i_max] << " and " << ids[j_max] << " with a score " << max_score << "!" << std::endl;
        std::cout << "Oracle: Out of available alleles, I'd say this person has " << ids[allele_ids.first] << " and " << ids[allele_ids.second] << " with a score " << max_score_short << "!" << std::endl;
      }

      std::cout << pn << " " << std::setw(19) << ids[allele_ids.first] << " " << std::setw(19) << ids[allele_ids.second] << std::endl;
      CharString outputFolder;

      if (CO.outputFolder == "")
      {
        outputFolder = "output/gyper/";
      }
      else
      {
        outputFolder = CO.outputFolder;
      }

      append(outputFolder, CO.gene);
      append(outputFolder, "/");
      append(outputFolder, pn);
      append(outputFolder, ".txt");

      std::stringstream my_ss;
      my_ss << ids[allele_ids.first] << "\t" << ids[allele_ids.second];

      if (CO.verbose)
      {
        std::cout << "Gyper output: " << outputFolder << std::endl;
      }
      
      writeToFile(my_ss, outputFolder);
    }
  }

  // Print VCF
  if (CO.vcf)
  {
    if (CO.verbose)
    {
      std::cout << "Number of 4 digit ids are: " << ids_four.size() << std::endl;
    }

    CharString vcfOutputFolder;
    if (CO.vcfOutputFolder == "")
    {
      vcfOutputFolder = "output/VCF/";
    }
    else
    {
      vcfOutputFolder = CO.vcfOutputFolder;
    }

    append(vcfOutputFolder, CO.gene);
    append(vcfOutputFolder, "/");
    append(vcfOutputFolder, pn);
    append(vcfOutputFolder, ".vcf");

    if (CO.verbose)
    {
      std::cout << "VCF output: " << vcfOutputFolder << std::endl;
    }

    writeVcfFile(vcfOutputFolder,
                 pn,
                 ids_four,
                 seq_scores_four,
                 max_score_short
                );
  }
}


int main (int argc, char const ** argv)   
{
  // Let's go!
  clock_t begin = clock();

  callOptions CO;

  ArgumentParser parser("gyper");
  addUsageLine(parser, "[\\fIOPTIONS\\fP] \"\\fIGENE\\fP\" \"\\fIBAMFILE\\fP\"");
  addDescription(parser, "Graph genotYPER. Maps sequencing data to possible alleles using a partial order graph.");

  // Requires at least one bam file as argument.
  addArgument(parser, ArgParseArgument(ArgParseArgument::STRING, "gene"));
  addArgument(parser, ArgParseArgument(ArgParseArgument::INPUT_FILE, "bamfile"));

  if (parseCommandLine(CO, parser, argc, argv) != ArgumentParser::PARSE_OK)
    return ArgumentParser::PARSE_ERROR;

  

  std::vector<std::string> ids;
  std::vector<VertexLabels> vertex_vector;
  boost::unordered_map< std::pair<TVertexDescriptor, TVertexDescriptor>, boost::dynamic_bitset<> > edge_ids;
  String<TVertexDescriptor> order;
  // std::vector<ExactBacktracker> backtracker1;
  // std::vector<ExactBacktracker> reverse_backtracker1;
  // std::vector<ExactBacktracker> backtracker2;
  // std::vector<ExactBacktracker> reverse_backtracker2;
  boost::unordered_set<TVertexDescriptor> free_nodes;
  free_nodes.insert(1);
  free_nodes.insert(0);
  TGraph graph;
  std::string gene(toCString(CO.gene));

  if (CO.exon_2_and_3)
  {
    create_exon_2_and_3_graph(CO, graph, vertex_vector, ids, edge_ids, free_nodes, order);
  }
  else
  {
    createGenericGraph(CO, graph, vertex_vector, ids, edge_ids, free_nodes, order);
  }
  
  printf("[%6.2f] Graph created.\n", double(clock()-begin) / CLOCKS_PER_SEC);

  TKmerMap kmer_map;
  TKmerMapSimple kmer_map_simple;
  bool simple_alignment = false;
  
  if (simple_alignment)
  {
    kmer_map_simple = kmerify_graph_simple(order, graph, vertex_vector, free_nodes, edge_ids, CO.kmer);
  }
  else
  {
    kmer_map = kmerifyGraph(order, graph, vertex_vector, free_nodes, edge_ids, CO.kmer);
  }

   
  
  printf("[%6.2f] Graph kmerification done.\n", double(clock()-begin) / CLOCKS_PER_SEC);

  std::string pn;
  std::string region = getRegion(CO, toCString(CO.gene));

  if (CO.verbose)
  {
    std::cout << "region = " << region << std::endl;
  }

  std::string bam_file_name_string(toCString(CO.bamFile));

  TBarMap bars1;
  TBarMap bars2;
  std::vector<std::string> bamlist;

  if (bam_file_name_string.size() >= 4 && bam_file_name_string.substr(bam_file_name_string.size()-4, 4) == ".bam")
  {
    // std::cout << "It is a bam file!" << std::endl;
    bamlist.push_back(bam_file_name_string);
  }
  else
  {
    // std::cout << "It is a bamlist!" << std::endl;
    std::ifstream bamlist_input(toCString(CO.bamFile));

    for (std::string line; getline( bamlist_input, line);)
    {
      bamlist.push_back(line);
    }
  }

  double total_alignment_time = 0.0;

  for (std::vector<std::string>::iterator bam_it = bamlist.begin() ; bam_it != bamlist.end() ; ++bam_it)
  {
    bars1.clear();
    bars2.clear();

    String<char> bam_file = bam_it->c_str();
    std::string bam_file_name_str(*bam_it);

    // std::cout << "Processing " << bam_file_name_str << std::endl;

    if (addToBars(CO, bars1, bars2, bam_file, region))
    {
      return 1;
    }

    // std::cout << "Bar sizes: " << bars1.size() << " " << bars2.size() << std::endl;
    std::string forward_slash("/");
  
    if (bam_file_name_str.find(forward_slash) != std::string::npos)
    {
      std::string::iterator str_it = std::find_end(bam_file_name_str.begin(), bam_file_name_str.end(), forward_slash.begin(), forward_slash.end())+1;
      char buffer[7];
      std::size_t length = bam_file_name_str.copy(buffer,7,str_it-bam_file_name_str.begin());
      buffer[length]='\0';
      pn.assign(buffer);
    }
    else
    {
      pn = bam_file_name_str.substr(0, 7);
    }

    if (CO.verbose)
    {
      std::cout << "bars1.size() = " << bars1.size();
      std::cout << ", bars2.size() = " << bars2.size() << std::endl;
      printf("[%6.2f] Read pairs filtered for %s.\n", double(clock()-begin) / CLOCKS_PER_SEC, pn.c_str());
    }
    
    std::vector<std::vector<double> > seq_scores;

    {
      std::vector<double> inner_seq_scores;
      inner_seq_scores.resize(ids.size());
      seq_scores.resize(ids.size(), inner_seq_scores);
    }

    double total_matches = 0.0;
    clock_t alignment_cpu_time = clock();

    for (TBarMap::iterator it = bars1.begin() ; it != bars1.end() ; ++it)
    {
      if (bars2.count(it->first) == 0)
      {
        continue;
      }

      String<Dna> sequence1 = it->second.seq;
      String<Dna> sequence2 = bars2[it->first].seq;

      // trimReadEnds(sequence1, it->second.qual, CO.bpQclip);
      // trimReadEnds(sequence2, bars2[it->first].qual, CO.bpQclip);
      // if (length(sequence1) < CO.minSeqLen[0] || length(sequence2) < CO.minSeqLen[0])
      // {
      //   continue;
      // }
      // std::cout << sequence1 << " " << sequence2 << std::endl;
      boost::dynamic_bitset<> ids_found1 = 
           align_sequence_kmer(sequence1,
                               it->second.qual,
                               ids.size(),
                               kmer_map,
                               vertex_vector,
                               CO.kmer,
                               CO.min_kmers
                              );

      if (ids_found1.none())
        continue;

      boost::dynamic_bitset<> ids_found2 = 
           align_sequence_kmer(sequence2,
                               bars2[it->first].qual,
                               ids.size(),
                               kmer_map,
                               vertex_vector,
                               CO.kmer,
                               CO.min_kmers
                              );

      if (ids_found2.none())
        continue;

      ids_found1 &= ids_found2;

      if (ids_found1.none())
        continue;

      total_matches += 1.0;

      // if (!ids_found1[2936])
      // {
      //   std::cout << sequence1 << "\n" << sequence2 << std::endl;
      // }
      
      // Full reference alleles
      for (unsigned i = 0 ; i < ids.size() ; ++i)
      {
        for (unsigned j = 0 ; j <= i ; ++j)
        {
          if (ids_found1[i] || ids_found1[j])
          {
            seq_scores[i][j] += 1.0;
          }
        }
      }
    }

    total_alignment_time += double(clock()-alignment_cpu_time) / CLOCKS_PER_SEC;

    if (CO.verbose)
    {
      printf("[%6.2f] Sequences aligned.\n", double(clock()-begin) / CLOCKS_PER_SEC);
    }

    for (std::vector<double>::iterator it = CO.beta.begin() ; it != CO.beta.end() ; ++it)
    {
      if (CO.verbose)
      {
        printf("[%6.2f] Output with beta = %.2f\n", 
          double(clock()-begin) / CLOCKS_PER_SEC,
          *it
        );
      }

      handleOutput(CO, *it, pn, ids, seq_scores, total_matches);
    }
  }

  printf("Total alignment cpu time was %6.2f.\n", total_alignment_time);
  printf("[%6.2f] Done.\n", double(clock()-begin) / CLOCKS_PER_SEC);
  return 0;
}
