# Makefile for Bitcoin Armory - C/C++ subdirectory Makefile
bin_PROGRAMS =
noinst_PROGRAMS =
lib_LTLIBRARIES =

DIST_SUBDIRS = lmdb fcgi cryptopp
SUBDIRS = lmdb fcgi cryptopp $(MAYBE_BUILD)

SWIG_FLAGS = -c++ -python -threads
AM_CXXFLAGS = $(CXXFLAGS)

if HAVE_GCC
SWIG_FLAGS += -D__GNUC__
if HAVE_64BIT
SWIG_FLAGS += -D__LP64__
endif
endif

if HAVE_CLANG
SWIG_FLAGS += -D__CLANG__
endif

INCLUDE_FILES = -Ibech32/ref/c++ -Icryptopp -Ifcgi

DB_SOURCE_FILES = main.cpp
CPPBLOCKUTILS_SOURCE_FILES = UniversalTimer.cpp BinaryData.cpp \
	BtcUtils.cpp DBUtils.cpp EncryptionUtils.cpp \
	BDM_seder.cpp DataObject.cpp FcgiMessage.cpp \
	SocketObject.cpp SwigClient.cpp StringSockets.cpp \
	BlockDataManagerConfig.cpp TxClasses.cpp \
	WalletManager.cpp Wallets.cpp Script.cpp Signer.cpp \
	Transactions.cpp CoinSelection.cpp ReentrantLock.cpp JSON_codec.cpp \
	TransactionBatch.cpp log.cpp TxEvalState.cpp \
	Accounts.cpp Addresses.cpp AssetEncryption.cpp Assets.cpp \
	DecryptedDataContainer.cpp DerivationScheme.cpp \
	bech32/ref/c++/bech32.cpp bech32/ref/c++/segwit_addr.cpp \
	lmdb_wrapper.cpp BlockObj.cpp BlockUtils.cpp \
	BtcWallet.cpp LedgerEntry.cpp ScrAddrObj.cpp Blockchain.cpp \
	BDM_mainthread.cpp BDM_supportClasses.cpp nodeRPC.cpp BDM_Server.cpp \
	BlockDataViewer.cpp HistoryPager.cpp Progress.cpp txio.cpp \
	StoredBlockObj.cpp DatabaseBuilder.cpp BlockchainScanner.cpp \
	BlockchainScanner_Super.cpp BlockDataMap.cpp BitcoinP2P.cpp  \
	SshParser.cpp

#ArmoryDB
bin_PROGRAMS += ArmoryDB
ArmoryDB_SOURCES = $(DB_SOURCE_FILES)
ArmoryDB_CXXFLAGS = $(AM_CXXFLAGS) -D__STDC_LIMIT_MACROS
ArmoryDB_CPPFLAGS = $(AM_CPPFLAGS) $(INCLUDE_FILES)
ArmoryDB_LDADD = fcgi/libfcgi/libfcgi.la \
		 cryptopp/libcryptopp.la \
		 lmdb/liblmdb.la \
		 libCppBlockUtils.la \
		 -lpthread
ArmoryDB_LDFLAGS = -static $(LDFLAGS)

#libCppBlockUtils - "shared" LDFLAG due to SWIG requirements
if HAVE_GUI
lib_LTLIBRARIES += libCppBlockUtils.la
libCppBlockUtils_la_SOURCES = $(CPPBLOCKUTILS_SOURCE_FILES) \
			      CppBlockUtils_wrap.cxx
libCppBlockUtils_la_CPPFLAGS = $(AM_CPPFLAGS) $(INCLUDE_FILES) \
		    		$(EXTRA_PYTHON_INCLUDES) $(AX_SWIG_PYTHON_CPPFLAGS)
libCppBlockUtils_la_CXXFLAGS = $(AM_CXXFLAGS) -Ilmdb \
				-Ifcgi/include \
				-D__STDC_LIMIT_MACROS
libCppBlockUtils_la_LIBADD = lmdb/liblmdb.la \
		 	 	cryptopp/libcryptopp.la \
		 	 	fcgi/libfcgi/libfcgi.la \
		 	 	$(PYTHON_LIBS)
		 	 	-lpthread
libCppBlockUtils_la_LDFLAGS = $(LDFLAGS) -shared

if BUILD_DARWIN
libCppBlockUtils_la_LDFLAGS += -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/
endif

#custom rules
CppBlockUtils_wrap.cxx: CppBlockUtils.i
	swig $(SWIG_FLAGS) CppBlockUtils.i 

.PHONY: CppBlockUtils_wrap.cxx

clean-local:
	rm -f CppBlockUtils.py
	rm -f CppBlockUtils_wrap.cxx
	rm -f CppBlockUtils_wrap.h
endif

# .include file prevents gtest subdir from building a second copy of Armory.
# Also provides a convenient spot to place all test-related materials.
if BUILD_TESTS
TESTS =
include Makefile.tests.include
endif
