#ifndef BS_CORE_HD_PATH_H
#define BS_CORE_HD_PATH_H

#include <string>
#include <set>
#include <vector>


namespace bs {
   namespace hd {

      class Path
      {
      public:
         using Elem = uint32_t;

         Path() {}
         Path(const std::vector<Elem> &elems);

         bool operator==(const Path &other) const {
            return (path_ == other.path_);
         }
         bool operator!=(const Path &other) const {
            return (path_ != other.path_);
         }
         bool operator < (const Path &other) const;

         void append(Elem elem, bool hardened = false);
         void append(const std::string &key, bool hardened = false);
         size_t length() const { return path_.size(); }
         Elem get(int index) const;   // negative index is an offset from end
         void clear();
         bool isAbolute() const { return isAbsolute_; }

         std::string toString(bool alwaysAbsolute = true) const;

         void setHardened(size_t index);
         bool isHardened(size_t index) const;

         static Path fromString(const std::string &);
         static Elem keyToElem(const std::string &key);
         static std::string elemToKey(Elem);

      private:
         std::vector<Elem> path_;
         std::set<size_t>  hardenedIdx_;
         bool isAbsolute_ = false;
      };


      static const Path::Elem purpose = 44;  // BIP44-compatible

      enum CoinType : Path::Elem {
         Bitcoin_main = 0,
         Bitcoin_test = 1,
         BlockSettle_CC = 0x4253,            // "BS" in hex
         BlockSettle_Auth = 0x41757468       // "Auth" in hex
      };

   }  //namespace hd
}  //namespace bs

#endif //BS_CORE_HD_PATH_H
