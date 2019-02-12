////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2019, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <string>
#include <iostream>
#include <sstream>

#include "btc/ecc.h"
#include "EncryptionUtils.h"
#include "AuthorizedPeers.h"

#define SERVER_FILE "server.peers"
#define CLIENT_FILE "client.peers"

using namespace std;
vector<string> names;

////////////////////////////////////////////////////////////////////////////////
pair<string, string> getKeyValFromLine(const string& line, char delim)
{
   stringstream ss(line);
   pair<string, string> output;

   //key
   getline(ss, output.first, delim);

   //val
   if (ss.good())
      getline(ss, output.second);

   return output;
}

////////////////////////////////////////////////////////////////////////////////
string stripQuotes(const string& input)
{
   size_t start = 0;
   size_t len = input.size();

   auto& first_char = input.c_str()[0];
   auto& last_char = input.c_str()[len - 1];

   if (first_char == '\"' || first_char == '\'')
   {
      start = 1;
      --len;
   }

   if (last_char == '\"' || last_char == '\'')
      --len;

   return input.substr(start, len);
}

map<string, string> parseArgs(int argc, char* argv[])
{
   map<string, string> args;
   for (int i = 1; i < argc; i++)
   {
      //check prefix
      if (strlen(argv[i]) < 2)
      {
         stringstream ss;
         ss << "argument #" << i << " is too short";
         throw runtime_error(ss.str());
      }

      string prefix(argv[i], 2);
      if (prefix != "--")
      {
         names.push_back(argv[i]);
      }

      //string prefix and tokenize
      string line(argv[i] + 2);
      auto&& argkeyval = getKeyValFromLine(line, '=');
      args.insert(make_pair(
         argkeyval.first, stripQuotes(argkeyval.second)));
   }

   return args;
}

int processArgs(map<string, string> args)
{
   //look for datadir
   string datadir("./");
   auto iter = args.find("datadir");
   if (iter != args.end())
      datadir = iter->second;

   //server or client?
   string filename;
   iter = args.find("server");
   if (iter != args.end())
      filename = SERVER_FILE;

   iter = args.find("client");
   if (iter != args.end())
   {
      if (filename.size() != 0)
         throw runtime_error("client/server setting conflict");
      filename = CLIENT_FILE;
   }

   if (filename.size() == 0)
      throw runtime_error("missing client or server argument!");

   AuthorizedPeers authPeers(datadir, filename);

   /*mutually exclusive args from here on*/

   //show my own public key
   iter = args.find("show-my-key");
   if (iter != args.end())
   {
      auto& ownkey = authPeers.getOwnPublicKey();
      BinaryDataRef bdr(ownkey.pubkey, 33);
      cout << "  displaying own public key (hex): " << bdr.toHexStr() << endl;
      return 0;
   }

   //show all keys
   iter = args.find("show-keys");
   if (iter != args.end())
   {
      map<BinaryDataRef, set<string>> keyToNames;
      auto& nameMap = authPeers.getPeerNameMap();
      for (auto& namePair : nameMap)
      {
         if (namePair.first == "own")
            continue;

         BinaryDataRef keyBdr(namePair.second.pubkey, 33);
         auto keyIter = keyToNames.find(keyBdr);
         if (keyIter == keyToNames.end())
         {
            auto keyPair = make_pair(keyBdr, set<string>());
            keyIter = keyToNames.insert(keyPair).first;
         }

         auto& nameSet = keyIter->second;
         nameSet.insert(namePair.first);
      }

      //intro
      cout << " displaying all keys in " << filename << ":" << endl;

      //output keys
      unsigned i = 1;
      for (auto& nameSet : keyToNames)
      {
         stringstream ss;
         ss << "  " << i << ". " << nameSet.first.toHexStr() << endl;
         ss << "   ";
         auto nameIter = nameSet.second.begin();
         while (true)
         {
            ss << "\"" << *nameIter++ << "\"";
            if (nameIter == nameSet.second.end())
               break;
            ss << ", ";
         }

         ss << endl;
         cout << ss.str();
         ++i;
      }

      return 0;
   }

   //add key
   iter = args.find("add-key");
   if(iter != args.end())
   {
      if (names.size() < 0)
         throw runtime_error("malformed add-key argument");

      BinaryData bd_key = READHEX(names[0]);
      if (bd_key.getSize() != 33 && bd_key.getSize() != 65)
         throw runtime_error("invalid public key size");

      if (!CryptoECDSA().VerifyPublicKeyValid(bd_key))
         throw runtime_error("invalid public key");

      SecureBinaryData key_compressed = bd_key;
      if (bd_key.getSize() == 65)
         key_compressed = CryptoECDSA().CompressPoint(bd_key);

      vector<string> keyNames;
      keyNames.insert(keyNames.end(), names.begin() + 1, names.end());
      authPeers.addPeer(key_compressed, keyNames);

      return 0;
   }

   cout << "no known command, aborting" << endl;
   return -1;
}

int main(int argc, char* argv[])
{
   btc_ecc_start();
   NetworkConfig::selectNetwork(NETWORK_MODE_MAINNET);

   map<string, string> args;
   try
   {
      args = parseArgs(argc, argv);
      return processArgs(args);
   }
   catch (exception& e)
   {
      cout << "failed to parse arguments with error: " << endl;
      cout << "   " << e.what() << endl;
   }

   cout << "no valid argument to process, exiting" << endl;
   return -1;
}