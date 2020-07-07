#ifndef READWRITEBIN_H
#define READWRITEBIN_H

#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <memory>
#include <utility>
#include <initializer_list>
#include <vector>
#include <iterator>
#include <sys/stat.h>
#include <type_traits>

// *******************************************
// *                                         *
// *            Read and write               *
// *                                         *
// *******************************************

template <typename T> class BinPtr;
template <typename T> class TypeBin;

class Bin {
  template <typename T> friend class BinPtr;
  template <typename T> friend class TypePtr;
  template <typename T> using iterator = BinPtr<T>;

 public:
  // The following class is simply a reference
  // type and it is used as default type for
  // some template functions
  class TypeNotSpecified {};
  using size_type = std::streamsize;

  // The destructor of the shared_ptr simply puts the pointer
  // to 0 in order to avoid infinite loop of destructors
  // (it would end up destroying itself more than once).
  explicit Bin(const std::string &filename, bool truncate = false, bool is_little_endian = true) :
      little_endian(is_little_endian), sptr(this, [] (Bin *p) { return p = 0; }) {
    struct stat buffer;
    bool already_exists = stat(filename.c_str(), &buffer) == 0;
    if (truncate || !already_exists)
      // Files are opened this way because otherwise the seekp
      // function wouldn't work.
      fs.open(filename,
               std::ios::out |
               std::ios::in |
               std::ios::ate | std::ios::trunc);
    else
      fs.open(filename,
               std::ios::out |
               std::ios::in |
               std::ios::ate);
    if (!fs.good() || !fs.is_open())
      throw std::domain_error("Couldn't open file!");

    rjump_to(0);
  }

  void rjump_to(std::streampos point) {
    // Jump to a location in the file to read.
    if (closed)
      throw std::domain_error("Can't jump and read closed file!");
    if (point > size())
      throw std::domain_error("Can't jump and read past EOF!");
    fs.seekg(point);
  }

  void wjump_to(std::streampos point) {
    // Jump to a location in the file to write. The only difference
    // I found between seekg and seekp is that the former doesn't
    // allow you to read past EOF.
    if (closed)
      throw std::domain_error("Can't jump and write on closed file!");
    fs.seekp(point);
  }

  // Getters
  size_type size() {
    // Get size of file
    if (closed)
      throw std::domain_error("Can't tell size of closed file!");
    auto p = fs.tellp();
    fs.seekp(0, std::ios::end);
    auto sz = fs.tellp();
    fs.seekp(p);
    return sz;
  }
  
  // Get the position you ar currently on (I haven't found a difference
  // yet)
  size_type wpos() { return fs.tellp(); }  // Write
  size_type rpos() { return fs.tellg(); }  // Read

  // Move by a certain number of steps, forward or backward. The size of
  // the step is deduced by the type specified
  template <typename T = char>
  void wmove_by(std::streamoff n) { fs.seekp(n * sizeof(T), std::ios::cur); }
  template <typename T = char>
  void rmove_by(std::streamoff n) { fs.seekg(n * sizeof(T), std::ios::cur); }

  /***********
   * WRITING *
   ***********/
  template <typename T> void write(T v) {
    // Write a value in the current position
    if (closed)
      throw std::domain_error("Can't write on closed file!");
    char *buf = reinterpret_cast<char*>(&v);
    if (!little_endian) std::reverse(buf, buf + sizeof(T));
    fs.write(buf, sizeof(T));
  }

  template <typename T> void write_many(T beg, T end) {
    // Write multiple values starting from the current position
    // given two iterators
    for (auto it = beg; it != end; ++it)
      write(*it);
  }

  template <typename K, typename T> void write_many(T beg, T end) {
    // Same as the function above, but here you can specify the
    // type you want to cast the values to
    for (auto it = beg; it != end; ++it)
      write<K>(*it);
  }

  template <typename K = Bin::TypeNotSpecified, typename T> void write_many(const std::initializer_list<T> &il) {
    /*
    // IN C++17 I WOULD HAVE DONE THE FOLLOWING
    if constexpr(std::is_same<K, Bin::TypeNotSpecified>::value) {
	    write_many(std::begin(il), std::end(il));
    } else {
	    write_many<K>(std::begin(il), std::end(il));
    }
    */
    is_initializer_list_cast_specified<K, T>(std::integral_constant<bool, std::is_same<K, Bin::TypeNotSpecified>::value>{}, il);
  }

  template <typename T>
  void write_many(const T &v) {
    // Pass the whole container
    for (auto it = std::begin(v); it != std::end(v); ++it)
      write(*it);
  }

  template <typename K, typename T>
  void write_many(const T &v) {
    // Cast the values
    for (auto it = std::begin(v); it != std::end(v); ++it)
      write<K>(*it);
  }

  /************************
   * Specify the location *
   ************************/
  // Check the functions above for details
  
  template <typename T> void write(T v, size_type p) {
    wjump_to(p);
    write(v);
  }

  template <typename K = Bin::TypeNotSpecified, typename T> void write_many(const std::initializer_list<T> &v, size_type p) {
    wjump_to(p);
    write_many<K>(v);
  }

  template <typename T> void write_many(T beg, T end, size_type p) {
    wjump_to(p);
    write_many(beg, end);
  }

  template <typename K, typename T> void write_many(T beg, T end, size_type p) {
    wjump_to(p);
    write_many<K>(beg, end);
  }

  template <typename T>
  void write_many(const T &v, size_type p) {
    wjump_to(p);
    for (auto it = std::begin(v); it != std::end(v); ++it)
      write(*it);
  }

  template <typename K, typename T>
  void write_many(const T &v, size_type p) {
    wjump_to(p);
    for (auto it = std::begin(v); it != std::end(v); ++it)
      write<K>(*it);
  }

  template <typename T> void operator=(T v) { write(v); }
  template <typename T> operator T() { return get_value<T>(); }

  void write_string(const std::string &v) {
    // Write a string to the current location
    if (closed)
      throw std::domain_error("Can't write string on closed file!");
    fs.write(v.data(), sizeof(char) * v.size());
  }
  void write_string(const std::string &v, size_type p) {
    // Write a string to the specified location
    wjump_to(p);
    write_string(v);
  }

  /***********
   * READING *
   ***********/
  
  template <typename T = unsigned char> T get_value() {
    // Get a value from the current location of the specified type
    if (closed)
      throw std::domain_error("Can't read from closed file!");
    if (static_cast<decltype(sizeof(T))>(size() - rpos()) < sizeof(T))
      throw std::runtime_error("Trying to read past EOF!");
    char buf[sizeof(T)];
    fs.read(buf, sizeof(T));
    // For float types, the behaviour of little and big endian is the same
    if (!little_endian && !std::is_floating_point<T>::value)
      std::reverse(&buf[0], &buf[sizeof(T)]);
    T *d = reinterpret_cast<T*>(buf);
    return *d;
  }

  template <typename T = unsigned char> std::vector<T> get_values(size_type n) {
    // Get multiple values from the current location
    if (closed)
      throw std::domain_error("Can't write on closed file!");
    if (static_cast<decltype(sizeof(T))>(size() - rpos()) < sizeof(T) * n)
      throw std::runtime_error("Trying to read past EOF!");
    char *buf = new char[sizeof(T) * n];
    fs.read(buf, sizeof(T) * n);
    std::vector<T> ret(n);

    if (!little_endian && !std::is_floating_point<T>::value) {
      for (int i = 0; i != n; ++i)
        std::reverse(&buf[i * sizeof(T)], &buf[(i + 1) * sizeof(T)]);
    }
    for (int i = 0; i != n; ++i)
      ret[i] = *reinterpret_cast<T*>(buf + (i * sizeof(T)));
    return ret;
  }

  template <typename T = unsigned char> T get_value(size_type p) {
    // Get a value from the specified location
    rjump_to(p);
    return get_value<T>();
  }

  template <typename T = unsigned char> std::vector<T> get_values(size_type n, size_type p) {
    // Get multiple values from the specified location
    rjump_to(p);
    return get_values<T>(n);
  }

  std::string get_string(std::string::size_type len) {
    // Read a string from the current location
    if (closed)
      throw std::domain_error("Can't read string from closed file!");
    if (len > static_cast<std::string::size_type>(size() - fs.tellg()))
      throw std::domain_error("Can't read string past EOF!");

    char *buf = new char[len + 1];
    fs.read(buf, len);
    buf[len] = '\0';
    std::string ret = buf;
    delete[] buf;
    return ret;
  }

  std::string get_string(std::string::size_type len, size_type p) {
    // Read a string from the specified location
    rjump_to(p);
    return get_string(len);
  }

  void flush() { fs.flush(); }

  void close() {
    fs.close();
    closed = true;
  }

  template <typename T> BinPtr<T> begin();
  template <typename T> BinPtr<T> end();

 private:
  std::fstream fs;
  bool little_endian;
  bool closed = false;
  std::shared_ptr<Bin> sptr;
  
  // The following two functions handle the case when the user passes
  // an initializer_list to write_many. The first one is called when
  // he doesn't specify a casting type, the second one is called when
  // he does. Check the function
  // template <typename K = Bin::TypeNotSpecified, typename T> write_many(const std::initializer_list<T> &il)
  // to see how they are called
  template <typename K, typename T>
  void is_initializer_list_cast_specified(std::true_type, const std::initializer_list<T>& il) {
    write_many(std::begin(il), std::end(il));
  }
  
  template <typename K, typename T>
  void is_initializer_list_cast_specified(std::false_type, const std::initializer_list<T>& il) {
    write_many<K>(std::begin(il), std::end(il));
  }


};

/*************** ITERATOR *******************/
/* +++++++++++++++ WARNING +++++++++++++++++
THE IMPLEMENTED ITERATOR IS EXTREMELY SLOWER.
IT IS STRONGLY DISCOURAGED ITS USE TO DEAL WITH
BIG FILES OR TO COMPUTE MANY OPERATIONS.
ITS USE IS RECOMMENDED FOR ELECANGE PURPOSE ONLY
+++++++++++++++++++++++++++++++++++++++++++++++ */


// The purpose if the following class is to
// treat differentiated BinPtr as they were
// reference to lvalue, which they clearly
// aren't

template <typename T>
class TypeBin {
  template <typename K> friend void swap(TypeBin<K> &a, TypeBin<K> &b);
  template <typename K> friend class BinPtr;

 public:
  explicit TypeBin(Bin &b, std::streamsize i) : tmp_b(b), curr(i) { }
  
  // Getting a value from a differentiated pointed
  operator T() & { return tmp_b.get_value<T>(curr); }
  
  // Setting a value to a differentiated pointer
  void operator=(T a) & { tmp_b.template write<T>(a, curr); }

 private:
  Bin &tmp_b;
  std::streamsize curr;

  void set_curr(std::streamsize i) { curr = i; }
};

template <typename T>
inline void swap(TypeBin<T> &a, TypeBin<T> &b) {
  T tmp = a.tmp_b.template get_value<T>(a.curr);
  a.tmp_b.template write<T>(b.tmp_b.template get_value<T>(b.curr), a.curr);
  b.tmp_b.template write<T>(tmp, b.curr);
}

// The actual pointer class
template <typename T>
class BinPtr {
  template <typename K> friend typename std::iterator_traits<BinPtr<K>>::difference_type operator-(const BinPtr<K> &a, const BinPtr<K> &b);
  template <typename K> friend bool operator<(const BinPtr<K> &ptr1, const BinPtr<K> &ptr2);

 public:
  using size_type = Bin::size_type;
  using value_type = T;

  BinPtr() : curr(0) { }
  explicit BinPtr(std::shared_ptr<Bin> &a, size_type sz = 0) : wptr(a), curr(sz), tb(*a, curr) { }

  TypeBin<T> &operator*() {
    auto p = check(0, "");
    p->wjump_to(curr);
    tb.set_curr(curr);
    return tb;
  }


  // Increment and decrement operators
  BinPtr &operator++() {
    check(0, "");  // This step decreases the speed by about 30%. Unfortunately it's needed
    curr += sizeof(T);
    return *this;
  }

  BinPtr &operator--() {
    if (curr < static_cast<long int>(sizeof(T)))
      throw std::out_of_range("decrement past begin of Bin");
    curr -= sizeof(T);
    return *this;
  }

  BinPtr operator++(int) {
    BinPtr ret = *this;
    ++*this;
    return ret;
  }

  BinPtr operator--(int) {
    BinPtr ret = *this;
    --*this;
    return ret;
  }

  BinPtr operator+(size_type n) const {
    auto b = check(0, "");
    return BinPtr(b, curr + sizeof(T) * n);
  }

  BinPtr operator-(size_type n) const {
    auto b = check(0, "");
    return BinPtr(b, curr - sizeof(T) * n);
  }

  // Relational operators
  bool operator==(const BinPtr &wrb2) const {
    auto b1 = wptr.lock();
    auto b2 = wrb2.wptr.lock();
    if (!b1 || !b2)
      throw std::runtime_error("comparing invalid BinPtr(s)");
    return curr == wrb2.curr &&
                   std::addressof(b1->fs) == std::addressof(b2->fs);
  }

  bool operator!=(const BinPtr &wrb2) const { return !(*this == wrb2); }

  // Since this class handles built-in types, I
  // don't want to allow the -> operator.
  BinPtr operator->() const = delete;

 private:
  std::weak_ptr<Bin> wptr;
  size_type curr;
  TypeBin<T> tb;

  std::shared_ptr<Bin> check(size_type i, const std::string &msg) const {
    // Various checks
    auto ret = wptr.lock();
    if (!ret)
      throw std::runtime_error("Unbound Bin");
    if (ret->closed)
      throw std::runtime_error("The file was closed!");
    if (i > ret->size())
      throw std::out_of_range(msg);
    return ret;
  }
};


template <typename T>
BinPtr<T> Bin::begin() { return BinPtr<T>(sptr); }

template <typename T>
BinPtr<T> Bin::end() { return BinPtr<T>(sptr, size()); }

template <typename T>
bool operator<(const BinPtr<T> &ptr1, const BinPtr<T> &ptr2) {
  return ptr1.curr < ptr2.curr;
}

template <typename T>
typename std::iterator_traits<BinPtr<T>>::difference_type operator-(const BinPtr<T> &a, const BinPtr<T> &b) {
  return (a.curr - b.curr) / sizeof(T);
}

// This "iterator_traits" part has been written with the only
// goal to make the code work, being aware that this could further affect
// negatively its already lacking cleanliness
namespace std {
    template <typename T>
    struct iterator_traits<BinPtr<T>> {
      typedef ptrdiff_t difference_type;
      typedef TypeBin<T>&& value_type;
      typedef TypeBin<T>&& reference;  // In general it whould be T&
      typedef BinPtr<T> pointer;  // In general it should be T*
      typedef std::random_access_iterator_tag iterator_category;
    };
}

#endif // READWRITEBIN_H
