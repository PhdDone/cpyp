#include <iostream>
#include <unordered_map>
#include <cstdlib>

#include "hpyplm.h"
#include "corpus/corpus.h"
#include "cpyp/m.h"
#include "cpyp/random.h"
#include "cpyp/crp.h"
#include "cpyp/tied_parameter_resampler.h"

#define kORDER 3

using namespace std;
using namespace cpyp;

Dict dict;

int main(int argc, char** argv) {
  if (argc != 4) {
    cerr << argv[0] << " <training.txt> <test.txt> <nsamples>\n\nEstimate a " << kORDER << "-gram HPYP LM and report perplexity\n100 is usually sufficient for <nsamples>\n";
    return 1;
  }
  MT19937 eng;
  string train_file = argv[1];
  string test_file = argv[2];
  int samples = atoi(argv[3]);
  
  vector<vector<unsigned> > corpuse;
  set<unsigned> vocabe, tv;
  const unsigned kSOS = dict.Convert("<s>");
  const unsigned kEOS = dict.Convert("</s>");
  cerr << "Reading corpus...\n";
  ReadFromFile(train_file, &dict, &corpuse, &vocabe);
  cerr << "E-corpus size: " << corpuse.size() << " sentences\t (" << vocabe.size() << " word types)\n";
  vector<vector<unsigned> > test;
  if (test_file.size()) ReadFromFile(test_file, &dict, &test, &tv); else test = corpuse;
  PYPLM<kORDER> lm(vocabe.size(), 1, 1, 1, 1);
  vector<unsigned> ctx(kORDER - 1, kSOS);
  for (int sample=0; sample < samples; ++sample) {
    for (const auto& s : corpuse) {
      ctx.resize(kORDER - 1);
      for (unsigned i = 0; i <= s.size(); ++i) {
        unsigned w = (i < s.size() ? s[i] : kEOS);
        if (sample > 0) lm.decrement(w, ctx, eng);
        lm.increment(w, ctx, eng);
        ctx.push_back(w);
      }
    }
    if (sample % 10 == 9) {
      cerr << " [LLH=" << lm.log_likelihood() << "]" << endl;
      if (sample % 30u == 29) lm.resample_hyperparameters(eng);
    } else { cerr << '.' << flush; }
  }
  double llh = 0;
  unsigned cnt = 0;
  unsigned oovs = 0;
  for (auto& s : test) {
    ctx.resize(kORDER - 1);
    for (unsigned i = 0; i <= s.size(); ++i) {
      unsigned w = (i < s.size() ? s[i] : kEOS);
      double lp = log(lm.prob(w, ctx)) / log(2);
      if (i < s.size() && vocabe.count(w) == 0) {
        cerr << "**OOV ";
        ++oovs;
        lp = 0;
      }
      cerr << "p(" << dict.Convert(w) << " |";
      for (unsigned j = ctx.size() + 1 - kORDER; j < ctx.size(); ++j)
        cerr << ' ' << dict.Convert(ctx[j]);
      cerr << ") = " << lp << endl;
      ctx.push_back(w);
      llh -= lp;
      cnt++;
    }
  }
  cnt -= oovs;
  cerr << "  Log_10 prob: " << (-llh * log(2) / log(10)) << endl;
  cerr << "        Count: " << cnt << endl;
  cerr << "         OOVs: " << oovs << endl;
  cerr << "Cross-entropy: " << (llh / cnt) << endl;
  cerr << "   Perplexity: " << pow(2, llh / cnt) << endl;
  return 0;
}

