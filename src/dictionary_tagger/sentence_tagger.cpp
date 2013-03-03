#include <algorithm>

#include "sentence_tagger.h"
#include "../nersuite_common/string_utils.h"

using namespace std;

namespace NER
{
	size_t SentenceTagger::max_ne_len = 10;
	int SentenceTagger::normalize_type = NormalizeNone;
	int SentenceTagger::overlap_resolution = OVL_TAG_LONGEST;

	vector<string> SentenceTagger::require_exact_POS = vector<string>();
	vector<string> SentenceTagger::require_prefix_POS = vector<string>();
	vector<string> SentenceTagger::disallow_exact_POS = vector<string>();
	vector<string> SentenceTagger::disallow_prefix_POS = vector<string>();
	bool SentenceTagger::filter_require_POS = false;
	bool SentenceTagger::filter_disallow_POS = false;

	SentenceTagger::SentenceTagger()
	{
		v_ne.reserve(256);
		v_idx.reserve(128);
		m_ContentType = 0;
	}

	size_t SentenceTagger::read(istream &is, const string &multidoc_separator)
	{
		m_Content.clear();		// clear the container
		m_ContentType = 0;

		string           line = "";
		vector<string>   line_items;

		while (! is.eof()) 
		{
			getline(is, line);

			// 1. Break if it reaches an empty line
			if (line.empty())
				break;

			// 2. Deal with the comments  
			if( multidoc_separator != "" && line.compare(0, multidoc_separator.length(), multidoc_separator) == 0 ) {
				if( set_content_type( 1 ) == false ) {
					exit(1);
				}

				// Save the line as it is
				line_items.clear();
				line_items.push_back( line );
				m_Content.push_back( line_items );

				continue;
			}

			// 3. Deal with a sentence
			if( set_content_type( 2 ) == false ) {
				exit(1);
			}

			tokenize(line_items, line, "\t");		// tokenize and
			m_Content.push_back(line_items);		// save it
		}
		
		return size();
	}

	void SentenceTagger::tag_nes(const Dictionary& dict)
	{
		v_ne.clear();
		v_idx.clear();

		for (size_t i_row = 0; i_row < size(); ++i_row)
		{
			vector<NE> nes;

			if ( normalize_type & NormalizeToken )
			{
				find_exact(i_row, nes, dict);
			}
			else
			{
				find_longest(i_row, nes, dict);
			}
			v_ne.insert(v_ne.end(), nes.begin(), nes.end());
		}
		resolve_collision();
		mark_ne(dict);
	}

	// Choose NEs to tag based on overlap resolution policy
	void SentenceTagger::resolve_collision()
	{
		int	re_beg = -1;
		size_t	num_ne = v_ne.size();

		for (size_t idx = 0; idx < num_ne; ++idx)
		{
		  	if (overlap_resolution == OVL_TAG_ALL || v_ne[idx].begin > re_beg)
			{
				v_idx.push_back(idx);
				re_beg = v_ne[idx].end;
			}
		}
	}

	void SentenceTagger::mark_ne(const Dictionary& dict)
	{
		int ori_n_col = (int) m_Content.front().size();
		size_t nclasses = dict.get_class_count();

		// 1) Create dictionary check columns
		for (V2_STR::iterator i_row = m_Content.begin(); i_row != m_Content.end(); ++i_row)
		{
			for (int k = 0; k < nclasses; ++k)
			{
				i_row->push_back("O");
			}
		}

		// 2) Fill matching information
		for (vector<int>::const_iterator itr = v_idx.begin(); itr != v_idx.end(); ++itr) 
		{
			// Tag for all classes that this NE belongs to
			for (list<string>::const_iterator cls_itr = v_ne[ *itr ].classes.begin();
				cls_itr != v_ne[ *itr ].classes.end();
				++cls_itr)
			{
				// 0) Get numeric class identifier and start position
				const int cls_num = atoi(cls_itr->c_str());
				int pos = v_ne[ *itr ].begin;

				// Non-"O" tag indicates a preceding multi-token NE
				// of the same class. Skip processing, giving preference
				// to leftmost NEs within each class.
				if ( m_Content[ pos ][ ori_n_col + cls_num ] != "O" )
				{
					continue;
				}

				// 1) Find a descriptive semantic class name
				const string&	sem_name = dict.get_class_name(cls_num);

				// 2) Label the data
				m_Content[ pos ][ ori_n_col + cls_num ] = "B-" + sem_name;

				for (pos = pos + 1; pos <= v_ne[ *itr ].end; ++pos)
				{
					m_Content[ pos ][ ori_n_col + cls_num ] = "I-" + sem_name;
				}
			}
		}
	}

	bool SentenceTagger::find_range(size_t i_row, size_t& min_len, size_t& max_len) const
	{
		min_len = 0;
		max_len = i_row + max_ne_len;

		// Find minimum length that includes required POS
		if (filter_require_POS && (min_len = find_min_length(i_row)) == (size_t)(-1))
		{
			return 0;
		}

		// Find maximum length that does not include disallowed POS
		if (filter_disallow_POS && (max_len = find_max_length(i_row)) == 0)
		{
			return 0;
		}
		
		// Protect against vector end boundary error
		if ((i_row + max_len) > size())
		{
			max_len = size() - i_row;
		}

		// Exclude last period
		if ((size() == (i_row + max_len)) && (m_Content.back()[RAW_TOKEN_COL] == "."))
		{
			--max_len;
		}

		if ( max_len > min_len )
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	string SentenceTagger::make_key(size_t i_row, size_t key_len) const
	{
		string key = m_Content[i_row][RAW_TOKEN_COL];

		for (int idx = 1; idx < key_len; ++idx)
		{
			if (m_Content[i_row + idx][BEG_COL] != m_Content[i_row + idx - 1][END_COL])
			{
				key += " ";  // Space between two tokens
			}
			key += m_Content[i_row + idx][RAW_TOKEN_COL];
		}

		return key;
	}

	int SentenceTagger::find_exact(size_t i_row, vector<NE>& nes, const Dictionary& dict) const
	{
		// Token-base matching --- try to match each single word of
		// the sentence with single token in the dictionary

		// Search dictionary
		string key = m_Content[i_row][RAW_TOKEN_COL];

		size_t count;
		const int *value = dict.get_classes(key, normalize_type, &count);
		if (value != NULL)
		{
			NE ne;
			ne.begin = i_row;
			ne.end = i_row;
			for (int i = 0; i < count; ++i)
			{
				ne.classes.push_back(int2str(value[i]));
			}
			ne.sim = 1.0;
			nes.push_back(ne);
			return 1;
		}
		return 0;
	}

	int SentenceTagger::find_longest(size_t i_row, vector<NE>& nes, const Dictionary& dict) const
	{
		// Normal matching --- try to find the longest sequence of
		// words that matches a dictionary entry

		size_t	key_min_len, key_max_len;
		
		find_range(i_row, key_min_len, key_max_len);

		// Search dictionary, longer candidate first
		size_t	key_len = key_max_len;
		for (; key_len > key_min_len; --key_len)
		{
			// 1) Make a key
			string key = make_key(i_row, key_len);

			// 2) Search Dictionary
			size_t count;
			const int *value = dict.get_classes(key, normalize_type, &count);
			if (value != NULL)
			{
				NE ne;
				ne.begin = i_row;
				ne.end = i_row + key_len - 1;	// Range is [begin, end]
				for (int i = 0; i < count; ++i)
				{
					ne.classes.push_back(int2str(value[i]));
				}
				ne.sim = 1.0;
				nes.push_back(ne);
				if ( overlap_resolution == OVL_TAG_LONGEST )
				{
					return key_len;  // Break when longest found
				}
			}
		}
		return 0;
	}

	size_t SentenceTagger::find_min_length(size_t i_row) const
	{
		size_t end = i_row + max_ne_len;

		if (end >= size())
		{
			end = size();
		}

		for (size_t col = i_row ; col < end; ++col)
		{
			// exact match
			if (find(require_exact_POS.begin(), require_exact_POS.end(),
				 m_Content[col][POS_COL]) != require_exact_POS.end())
			{
				return col - i_row;
			}			   
			// prefix match
			for (vector<string>::iterator i = require_prefix_POS.begin();
			     i != require_prefix_POS.end(); ++i)
			{
				if (m_Content[col][POS_COL].substr(0, (*i).length()) == *i)
				{
					return col - i_row;
				}
			}
		}
		return (size_t)(-1);
	}

	size_t SentenceTagger::find_max_length(size_t i_row) const
	{
		size_t end = i_row + max_ne_len;

		if (end >= size())
		{
			end = size();
		}

		for (size_t col = i_row ; col < end; ++col)
		{
			// exact match
			if (find(disallow_exact_POS.begin(), disallow_exact_POS.end(),
				 m_Content[col][POS_COL]) != disallow_exact_POS.end())
			{
				return col - i_row;
			}			   
			// prefix match
			for (vector<string>::iterator i = disallow_prefix_POS.begin();
			     i != disallow_prefix_POS.end(); ++i)
			{
				if (m_Content[col][POS_COL].substr(0, (*i).length()) == *i)
				{
					return col - i_row;
				}
			}
		}
		return end - i_row;
	}
}
