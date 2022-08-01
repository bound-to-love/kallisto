#ifndef KALLISTO_KMERINDEX_H
#define KALLISTO_KMERINDEX_H

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <stdint.h>
#include <iostream>
#include <numeric>
#include <limits>

#include "common.h"
#include "Kmer.hpp"
#include "hash.hpp"
#include "CompactedDBG.hpp"
#include "Node.hpp"

std::string revcomp(const std::string s);

struct TRInfo {
  uint32_t trid;
  // denotes where the transcript begins, with respect to a given contig
  uint32_t start;
  // denotes where the transcript ends, with respect to a given contig
  // exclusive [start,stop)
  uint32_t stop;
  // denotes where the given contig starts with respect to the transcript
  uint32_t pos;
  bool sense; // true for sense, false for anti-sense
};

using EcMap = std::vector<Roaring>; //std::unordered_map<int, std::vector<int>>;

struct SortedVectorHasher {
  size_t operator()(const std::vector<int>& v) const {
    uint64_t r = 0;
    int i=0;
    for (auto x : v) {
      uint64_t t;
      MurmurHash3_x64_64(&x,sizeof(x), 0,&t);
      t = (x>>i) | (x<<(64-i));
      r = r ^ t;
      i = (i+1)%64;
    }
    return r;
  }
};

struct RoaringHasher {
  size_t operator()(const Roaring& r) const {
    uint32_t* trs = new uint32_t[r.cardinality()];
    r.toUint32Array(trs);
    uint64_t h = 0;
    int i=0;
    for (size_t j = 0; j < r.cardinality(); ++j) {
      uint64_t t;
      MurmurHash3_x64_64(&trs[j], sizeof(trs[j]), 0, &t);
      t = (trs[j]>>i) | (trs[j]<<(64-i));
      h = h ^ t;
      i = (i+1)%64;
    }
    delete[] trs;
    trs = nullptr;
    return h;
  }
};

struct KmerEntry {
  int32_t contig; // id of contig
  uint32_t _pos; // 0-based forward distance to EC-junction
  int32_t contig_length;

  KmerEntry() : contig(-1), _pos(0xFFFFFFF), contig_length(0) {}
  KmerEntry(int id, int length, int pos, bool isFw) : contig(id), contig_length(length) {
    setPos(pos);
    setDir(isFw);
  }

  inline int getPos() const {return (_pos & 0x0FFFFFFF);}
  inline int isFw() const  {return (_pos & 0xF0000000) == 0; }
  inline void setPos(int p) {_pos = (_pos & 0xF0000000) | (p & 0x0FFFFFFF);}
  inline void setDir(bool _isFw) {_pos = (_pos & 0x0FFFFFFF) | ((_isFw) ? 0 : 0xF0000000);}
  inline int getDist(bool fw) const {
    if (isFw() == fw) {
      return (contig_length - 1 - getPos());
    } else {
      return getPos();
    }
  }
};

struct ContigToTranscript {
  int trid;
  int pos;
  bool sense; // true for sense,
};

struct Contig {
  int id; // internal id
  int length; // number of k-mers
  int ec;
  std::string seq; // sequence
  std::vector<ContigToTranscript> transcripts;
};

struct DBGraph {
  std::vector<int> ecs; // contig id -> ec-id
  std::vector<Contig> contigs; // contig id -> contig
//  std::vector<pair<int, bool>> edges; // contig id -> edges
};

struct KmerIndex {
  KmerIndex(const ProgramOptions& opt) : k(opt.k), num_trans(0), skip(opt.skip), target_seqs_loaded(false) {
    //LoadTranscripts(opt.transfasta);
  }

  ~KmerIndex() {}

  void match(const char *s, int l, std::vector<std::pair<const_UnitigMap<Node>, int>>& v) const;
//  bool matchEnd(const char *s, int l, std::vector<std::pair<int, int>>& v, int p) const;
  int mapPair(const char *s1, int l1, const char *s2, int l2) const;
  Roaring intersect(const Roaring& ec, const Roaring& v) const;

  void BuildTranscripts(const ProgramOptions& opt);
  void BuildDeBruijnGraph(const ProgramOptions& opt, const std::string& tmp_file);
  void BuildEquivalenceClasses(const ProgramOptions& opt, const std::string& tmp_file);
  //void BuildDeBruijnGraph(const ProgramOptions& opt, const std::vector<std::string>& seqs);
  //void BuildEquivalenceClasses(const ProgramOptions& opt, std::vector<std::string>& seqs);
  //void FixSplitContigs(const ProgramOptions& opt, std::vector<std::vector<TRInfo>>& trinfos);
  void PopulateMosaicECs(std::vector<std::vector<TRInfo> >& trinfos);

  // output methods
  void write(const std::string& index_out, bool writeKmerTable = true, int threads = 1);
  void writePseudoBamHeader(std::ostream &o) const;

  // note opt is not const
  // load methods
  void load(ProgramOptions& opt, bool loadKmerTable = true);
  void loadTranscriptSequences() const;
  void loadECsFromFile(const ProgramOptions& opt);
  void loadTranscriptsFromFile(const ProgramOptions& opt);
  void clear();

  // positional information
  std::pair<int,bool> findPosition(int tr, Kmer km, const_UnitigMap<Node>& um, int p = 0) const;
  std::pair<int,bool> findPosition(int tr, Kmer km, int p) const;

  int k; // k-mer size used
  int num_trans; // number of targets
  int skip;

  //KmerHashTable<KmerEntry, KmerHash> kmap;
  CompactedDBG<Node> dbg;
  EcMap ecmap;
  DBGraph dbGraph;
  std::unordered_map<Roaring, int32_t, RoaringHasher> ecmapinv;
  const size_t INDEX_VERSION = 12; // increase this every time you change the file format

  std::vector<uint32_t> target_lens_;

  std::vector<std::string> target_names_;
  std::vector<std::string> target_seqs_; // populated on demand
  bool target_seqs_loaded;
};

#endif // KALLISTO_KMERINDEX_H
