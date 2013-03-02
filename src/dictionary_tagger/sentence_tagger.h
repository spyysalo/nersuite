/*
*      SentenceTagger matcher
*
* Copyright (c) 
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the names of the authors nor the names of its contributors
*       may be used to endorse or promote products derived from this
*       software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
* OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef _SENTENCE_TAGGER_H
#define _SENTENCE_TAGGER_H

#include <vector>
#include <string>
#include <iostream>
#include "../nersuite_common/dictionary.h"
#include "../nersuite_common/ne.h"

// Predefined column info.
#define		BEG_COL			0
#define		END_COL			1
#define		RAW_TOKEN_COL	2
#define		POS_COL			4

namespace NER
{
	/** 
	* @ingroup NERsuite
	*/

	/**
	* Sentence Tagger - internal class used to represent the sentence-token-feature array structure
	*
	* This class has two main functionarities: reading a sentence to construct the structure,
	* and appending Dictionary-class features to the structure.
	* 
	* This class reads a sentence block from a stream and creates a list of tokens. 
	* Each token consists of a list of features.
	* After reading a sentence, the function @ref tag_nes() will be used to append
	* extra features which are retrieved from the given Dictionary.
	*/
	class SentenceTagger
	{
	private:
		typedef std::vector< std::vector<std::string> >	V2_STR;

		static size_t	max_ne_len;
		static int		normalize_type;

		// POS filter-related data (see set_POS_filter())
		static std::vector<std::string> require_exact_POS;
		static std::vector<std::string> require_prefix_POS;
		static std::vector<std::string> disallow_exact_POS;
		static std::vector<std::string> disallow_prefix_POS;
		static bool filter_require_POS;
		static bool filter_disallow_POS;

		// Sentence Data (Tokenized array)
		V2_STR	m_Content;
		int     m_ContentType;     // 0: initial value, 1: comment, 2: sentence

		std::vector<NE>		v_ne;

		std::vector<int>	v_idx;

	public:
		/**
		* Constructs a SentenceTagger object
		*/
		SentenceTagger();

		/**
		* Destroys a SentenceTagger object
		*/
		virtual ~SentenceTagger() {}

		/**
		* Set Normalization Type used while a SentenceTagger queries the dictionary
		* @param[in] nt A combination of Normalization Types (OR of NormalizeType).
		*/
		static void set_normalize_type(int nt) { normalize_type = nt; }

		/**
		* Set candidate sequence POS tag filter.  Only sequences
		* containing a POS tag matching any in tag in in_exact or in_prefix
		* and not containing a POS matching any tag in out_exact or out_prefix
		* are considered in tagging. Matching against the _prefix list tags is
		* prefix only, so e.g. "NN" in in_prefix matches "NNS".
		* @param[in] require_exact Require POS tag matching any
		* @param[in] require_prefix Require POS tag beginning with any
		* @param[in] out_exact Disallow POS tag matching any
		* @param[in] out_prefix Disallow POS tag beginning with any
		*/
		static void set_POS_filter(const std::vector<std::string>& require_exact = std::vector<std::string>(),
					   const std::vector<std::string>& require_prefix = std::vector<std::string>(),
					   const std::vector<std::string>& disallow_exact = std::vector<std::string>(),
					   const std::vector<std::string>& disallow_prefix = std::vector<std::string>()) {
			require_exact_POS = require_exact;
			require_prefix_POS = require_prefix;
			disallow_exact_POS = disallow_exact;
			disallow_prefix_POS = disallow_prefix;

			filter_require_POS = require_exact.size() > 0 || require_prefix.size() > 0;
			filter_disallow_POS = disallow_exact.size() > 0 || disallow_prefix.size() > 0;
		}

		/**
		* Get the size of sentence which this object is currentry processing.
		* @return Returns the size of sentence (Count of tokens).
		*/
		size_t	size() const { return m_Content.size(); }

		/**
		* Test if the token list is empty.
		* This function can be used to test whether the input read was an empty line.
		* @return Returns true if empty.
		*/
		bool	empty() const { return m_Content.empty(); }

		/**
		* Test the type of m_Content.
		* @return 0: not initialized, 1: comment, 2: sentence
		*/
		int	get_content_type() const { 
			return m_ContentType;
		}

		/**
		* Set the content type. 
		* @return Returns true if it has one content type, otherwise false
		*/
		bool	set_content_type(int _type) {
			if( (m_ContentType == 0 || m_ContentType == 1) && (_type == 1) ) {
				m_ContentType = 1;
				return true;
			}else if( (m_ContentType == 0 || m_ContentType == 2) && (_type == 2) ) {
				m_ContentType = 2;
				return true;
			}	else {
				std::cerr << "Error: Input data format: Comments and sentences must be separated by a blank line." << std::endl;
				return false;
			}
		}

		/**
		* The begin() iterator for the internal token list
		*/
		V2_STR::iterator	begin() { return m_Content.begin(); }

		/**
		* The end() iterator for the internal token list
		*/
		V2_STR::iterator	end() { return m_Content.end(); }

		/**
		* The array indexer operator for the internal token list
		*/
		V1_STR&	operator[](size_t index) { return m_Content[index]; }

		/**
		* Read a sentence from the given stream and create the internal token list.
		* @param[in] multidoc_separator String marking document break (if non-empty)
		* @return Size of tokens read
		*/
		size_t	read(std::istream &ifs, const std::string &multidoc_separator="");

		/**
		* Append dictionary-class features to the internal token list.
		* @param[in] dict The Dictionary used to search for feature classes.
		*/
		void	tag_nes(const Dictionary& dict);

	private:
		void	resolve_collision();

		void	mark_ne(const Dictionary& dict);

		int		find_longest(size_t i_row, NE& ne, const Dictionary& dict) const;

		int		find_exact(size_t i_row, NE& ne, const Dictionary& dict) const;

		/* Return minimum length of sequence satisfying required POS filter. */
		size_t	find_min_length(size_t i_row) const;

		/* Return maximum length of sequence not violating disallowed POS filter. */
		size_t	find_max_length(size_t i_row) const;

	};
}

#endif
