// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#include "Checkpoints.h"
#include "Common/StringTools.h"
#include <cstdlib>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <resolv.h>
#include <cstring>
#include <string>
#include <string.h>
#include <sstream>
#include <vector>
#include <iterator>
#include <boost/algorithm/string.hpp>

using namespace Logging;

namespace
{
  bool dns_records_match(const std::vector<std::string>& a, const std::vector<std::string>& b)
  {
    if (a.size() != b.size()) return false;

    for (const auto& record_in_a : a)
    {
      bool ok = false;
      for (const auto& record_in_b : b)
      {
	if (record_in_a == record_in_b)
	{
	  ok = true;
	  break;
	}
      }
      if (!ok) return false;
    }

    return true;
  }
} // anonymous namespace

namespace CryptoNote {
//---------------------------------------------------------------------------
Checkpoints::Checkpoints(Logging::ILogger &log) : logger(log, "checkpoints") {}
//---------------------------------------------------------------------------
bool Checkpoints::add_checkpoint(uint32_t height, const std::string &hash_str) {
  Crypto::Hash h = NULL_HASH;
  if (!Common::podFromHex(hash_str, h)) {
    logger(WARNING) << "Wrong hash in checkpoint for height " << height;
    return false;
  }
  if (!(0 == m_points.count(height))) {
    logger(WARNING) << "Checkpoint already exists.";
    return false;
  }

  m_points[height] = h;

  return true;
}
//---------------------------------------------------------------------------
bool Checkpoints::is_in_checkpoint_zone(uint32_t height) const {
  if(m_points.empty()) {
    logger(Logging::INFO, Logging::GREEN) << "m_points EMPTY";
  }
  return !m_points.empty() && (height <= (--m_points.end())->first);
}
//---------------------------------------------------------------------------
bool Checkpoints::check_block(uint32_t height, const Crypto::Hash &h, bool &is_a_checkpoint) const {
  auto it = m_points.find(height);
  is_a_checkpoint = it != m_points.end();
  if (!is_a_checkpoint)
    return true;

  if (it->second == h) {
    logger(Logging::INFO, Logging::GREEN) << "CHECKPOINT PASSED FOR HEIGHT " << height << " " << h;
    return true;
  } else {
    logger(Logging::ERROR) << "CHECKPOINT FAILED FOR HEIGHT " << height
                           << ". EXPECTED HASH: " << it->second
                           << ", FETCHED HASH: " << h;
    return false;
  }
}
//---------------------------------------------------------------------------
bool Checkpoints::check_block(uint32_t  height, const Crypto::Hash &h) const {
  bool ignored;
  return check_block(height, h, ignored);
}
//---------------------------------------------------------------------------
bool Checkpoints::is_alternative_block_allowed(uint32_t  blockchain_height,
                                               uint32_t  block_height) const {
  if (0 == block_height)
    return false;

  auto it = m_points.upper_bound(blockchain_height);
  // Is blockchain_height before the first checkpoint?
  if (it == m_points.begin())
    return true;

  --it;
  uint32_t  checkpoint_height = it->first;
  return checkpoint_height < block_height;
}

std::vector<uint32_t> Checkpoints::getCheckpointHeights() const {
  std::vector<uint32_t> checkpointHeights;
  checkpointHeights.reserve(m_points.size());
  for (const auto& it : m_points) {
    checkpointHeights.push_back(it.first);
  }

  return checkpointHeights;
}

bool Checkpoints::load_checkpoints_from_dns()
{
  u_char nsbuf[4096], dispbuf[4096];
  ns_msg msg;
  ns_rr rr;
  int i, j, l;
  std::string domain("checkpoints.niobiocash.money");
  l = res_query(domain.c_str(), ns_c_any, ns_t_txt, nsbuf, sizeof (nsbuf));
  if (l < 0) return false;
  ns_initparse(nsbuf, l, &msg);
  l = ns_msg_count(msg, ns_s_an);
  for (j = 0; j < l; j++) {
    int prr = ns_parserr(&msg, ns_s_an, j, &rr);
    ns_sprintrr(&msg, &rr, NULL, NULL, reinterpret_cast<char*> (dispbuf), sizeof (dispbuf));
    char s1[255];
    char s2[255];
    char s3[255];
    char s4[255];
    char s5[255];
    std::stringstream ss;
    ss = std::stringstream(reinterpret_cast<char*> (dispbuf));
    ss >> s1;
    ss >> s2;
    ss >> s3;
    ss >> s4;
    ss >> s5;
    std::vector< std::string > results;
    std::string checkpointStr(s5);
    boost::split(results, checkpointStr, boost::is_any_of(":\""));

    uint64_t height;
    Crypto::Hash hash;

    // parse the first part as uint64_t,
    // if this fails move on to the next record
    ss = std::stringstream(results[1]);
    if (!(ss >> height)) continue;

    // parse the second part as crypto::hash,
    // if this fails move on to the next record
    if (!Common::podFromHex(results[2], hash)) continue;

    if(m_points.count(height) > 0) {
      logger(INFO) << "Checkpoint exists for height: " << height << ". Ignoring dns checkpoint.";
    } else {
      add_checkpoint(height, results[2]);
    }
  }
  return true;
}
}
