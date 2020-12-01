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
  /*! An empty class.
   *
   * This class is used as default type for
   * some template functions and it is used to understand
   * if the template parameter is omitted.
   */
  class TypeNotSpecified {};

  //! The type used to indicate positions inside the file
  using size_type = std::streamsize;

  // The destructor of the shared_ptr simply puts the pointer
  // to 0 in order to avoid infinite loop of destructors
  // (it would end up destroying itself more than once).
  /*! The constructor
   *
   * The destructor of the shared_ptr simply puts the pointer
   * to 0 in order to avoid infinite loop of destructors
   * (it would end up destroying itself more than once).
   *
   * \param fname             The filename. If the file doesn't exist it is created
   * \param truncate          If set to true and the file already exists it is cleared. The default value is false.
   * \param use_little_endian
   * \parblock
   * Decide if you want to read/write in little_endian.
   * By default it is set to the default endianness of the machine.
   * \endparblock
   */
  explicit Bin(const std::string &fname, bool truncate = false, bool use_little_endian = Bin::is_default_little_endian()) :
      filename(fname), sptr(this, [] (Bin *p) { return p = 0; }) {
    opposite_endian = use_little_indian != Bin::is_default_little_endian();
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

  /*! Tells if the machine is little endian (true)
   * or big endian (false)
   */
  static bool is_default_little_endian() {
    short int n = 1;
    char *address = reinterpret_cast<char*>(&n);
    if (*(address + 1) == 1) return false;
    else if (*address == 1) return true;
    throw std::domain_error("Can't detect endianness of the machine!");
  }

  /*! Returns the number of bytes occupied by n_steps instances of T
   * \tparam T The type T
   * \param n_steps The number of instances of an object of type T
   */
  template <typename T> static constexpr size_type bytes(size_type n_steps) {
    return sizeof(T) * n_steps;
  }

  /*! Jump to a location in the file to read
   *
   * \param point The point (in bytes) where you want to jump
   */
  void rjump_to(std::streampos point) {
    if (closed)
      throw std::domain_error("Can't jump and read closed file!");
    if (point > size())
      throw std::domain_error("Can't jump and read past EOF!");
    fs.seekg(point);
  }

  /*! Jump to a location in the file to write
   *
   * The only difference I found between seekg and seekp is
   * that the former doesn't allow you to read past EOF.
   *
   * \param point The point (in bytes) where you want to jump
   */
  void wjump_to(std::streampos point) {
    if (closed)
      throw std::domain_error("Can't jump and write on closed file!");
    fs.seekp(point);
  }

  // Getters

  /*! Get the size of the file */
  size_type size() {
    if (closed)
      throw std::domain_error("Can't tell size of closed file!");
    auto p = fs.tellp();
    fs.seekp(0, std::ios::end);
    auto sz = fs.tellp();
    fs.seekp(p);
    return sz;
  }
  
  /*! Get the position you ar currently on (write) */
  size_type wpos() { return fs.tellp(); }

  /*! Get the position you ar currently on (read) */
  /*! It seems to be identical to the write version */
  size_type rpos() { return fs.tellg(); }

  /*! Move by a certain number of steps, forward or backward.
   * The size of the step is deduced by the type specified
   * \param n The number of steps
   */
  template <typename T = char>
  void wmove_by(std::streamoff n) { fs.seekp(bytes<T>(n), std::ios::cur); }

  /*! Move by a certain number of steps, forward or backward.
   * The size of the step is deduced by the type specified
   * \param n The number of steps
   */
  template <typename T = char>
  void rmove_by(std::streamoff n) { fs.seekg(bytes<T>(n), std::ios::cur); }

  /***********
   * WRITING *
   ***********/

  /*! Write a value in the current position
   * \param v The value you want to write
   */
  template <typename T> void write(T v) {
    if (closed)
      throw std::domain_error("Can't write on closed file!");
    char *buf = reinterpret_cast<char*>(&v);
    if (opposite_endian) std::reverse(buf, buf + sizeof(T));
    fs.write(buf, sizeof(T));
  }

  /*! Write multiple values starting from the current position
   * given two iterators.
   * \param beg The beginning interator
   * \param end The ending interator
   */
  template <typename T> void write_many(T beg, T end) {
    for (auto it = beg; it != end; ++it)
      write(*it);
  }

  /*! Write multiple values starting from the current position
   * given two iterators.
   * If you want, this implementation allows you to specify the
   * type you want to cast the values to
   * \param beg The beginning interator
   * \param end The ending interator
   */
  template <typename K, typename T> void write_many(T beg, T end) {
    for (auto it = beg; it != end; ++it)
      write<K>(*it);
  }

  /*! Write multiple values starting from the current position
   * given an initializer list.
   * If you want, this implementation allows you to specify the
   * type you want to cast the values to
   * \param il The initializer list
   */
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

  /*! Write multiple values starting from the current position
   * given a container.
   * \param v The container
   */
  template <typename T>
  void write_many(const T &v) {
    for (auto it = std::begin(v); it != std::end(v); ++it)
      write(*it);
  }


  /*! Write multiple values starting from the current position
   * given a container.
   * If you want, this implementation allows you to specify the
   * type you want to cast the values to
   * \param v The container
   */
  template <typename K, typename T>
  void write_many(const T &v) {
    for (auto it = std::begin(v); it != std::end(v); ++it)
      write<K>(*it);
  }

  /**************************************
   * Specify the location where to write*
   **************************************/
  // Check the functions above for details
 

  /*! Write a value in the specified position
   * \param v The value you want to write
   * \param p The position where you want to write
   */
  template <typename T> void write(T v, size_type p) {
    wjump_to(p);
    write(v);
  }


  /*! Write multiple values starting from the specified position
   * given an initializer list.
   * If you want, this implementation allows you to specify the
   * type you want to cast the values to
   * \param beg The beginning interator
   * \param end The ending interator
   * \param p The position where you want to write
   */
  template <typename K = Bin::TypeNotSpecified, typename T> void write_many(const std::initializer_list<T> &v, size_type p) {
    wjump_to(p);
    write_many<K>(v);
  }


  /*! Write multiple values starting from the specified position
   * given two iterators.
   * \param beg The beginning interator
   * \param end The ending interator
   * \param p The position where you want to write
   */
  template <typename T> void write_many(T beg, T end, size_type p) {
    wjump_to(p);
    write_many(beg, end);
  }


  /*! Write multiple values starting from the specified position
   * given two iterators.
   * If you want, this implementation allows you to specify the
   * type you want to cast the values to
   * \param beg The beginning interator
   * \param end The ending interator
   * \param p The position where you want to write
   */
  template <typename K, typename T> void write_many(T beg, T end, size_type p) {
    wjump_to(p);
    write_many<K>(beg, end);
  }


  /*! Write multiple values starting from the current position
   * given a container.
   * \param v The container
   * \param p The position where you want to write
   */
  template <typename T>
  void write_many(const T &v, size_type p) {
    wjump_to(p);
    for (auto it = std::begin(v); it != std::end(v); ++it)
      write(*it);
  }


  /*! Write multiple values starting from the current position
   * given a container.
   * If you want, this implementation allows you to specify the
   * type you want to cast the values to
   * \param v The container
   * \param p The position where you want to write
   */
  template <typename K, typename T>
  void write_many(const T &v, size_type p) {
    wjump_to(p);
    for (auto it = std::begin(v); it != std::end(v); ++it)
      write<K>(*it);
  }

  /*! Assign operator.
   * It is used to write a in the file the value assigned
   */
  template <typename T> void operator=(T v) { write(v); }

  /*! Casting operator
   * It is used to read from the file a value casted from
   * a certain type.
   */
  template <typename T> operator T() { return get_value<T>(); }

  /*! Write a string in the current position
   * \param v The string you want to write
   */
  void write_string(const std::string &v) {
    if (closed)
      throw std::domain_error("Can't write string on closed file!");
    fs.write(v.data(), bytes<char>(v.size()));
  }

  /*! Write a string in the specified position
   * \param v The string you want to write
   * \param p The position where you want to write
   */
  void write_string(const std::string &v, size_type p) {
    wjump_to(p);
    write_string(v);
  }

  /***********
   * READING *
   ***********/
  
  /*! Read a single value of type T from the current position */
  template <typename T = unsigned char> T get_value() {
    if (closed)
      throw std::domain_error("Can't read from closed file!");
    if (static_cast<decltype(sizeof(T))>(size() - rpos()) < sizeof(T))
      throw std::runtime_error("Trying to read past EOF!");
    char buf[sizeof(T)];
    fs.read(buf, sizeof(T));
    // For float types, the behaviour of little and big endian is the same
    if (opposite_endian && !std::is_floating_point<T>::value)
      std::reverse(&buf[0], &buf[sizeof(T)]);
    T *d = reinterpret_cast<T*>(buf);
    return *d;
  }

  /*! Read multiple values of type T from the current position
   * \param n The number of elements of type T you want to read
   */
  template <typename T = unsigned char> std::vector<T> get_values(size_type n) {
    if (closed)
      throw std::domain_error("Can't write on closed file!");
    if (static_cast<decltype(sizeof(T))>(size() - rpos()) < bytes<T>(n))
      throw std::runtime_error("Trying to read past EOF!");
    char *buf = new char[bytes<T>(n)];
    fs.read(buf, bytes<T>(n));
    std::vector<T> ret(n);

    if (opposite_endian && !std::is_floating_point<T>::value) {
      for (int i = 0; i != n; ++i)
        std::reverse(&buf[bytes<T>(i)], &buf[bytes<T>(i + 1)]);
    }
    for (int i = 0; i != n; ++i)
      ret[i] = *reinterpret_cast<T*>(buf + bytes<T>(i));
    return ret;
  }


  /*! Read a single value of type T from the specified position
   * \param p The position from where you want to read
   */
  template <typename T = unsigned char> T get_value(size_type p) {
    rjump_to(p);
    return get_value<T>();
  }

  /*! Read multiple values of type T from the specified position
   * \param n The number of elements of type T you want to read
   * \param p The position from where you want to read
   */
  template <typename T = unsigned char> std::vector<T> get_values(size_type n, size_type p) {
    // Get multiple values from the specified location
    rjump_to(p);
    return get_values<T>(n);
  }

  /*! Read a string from the current location */
  std::string get_string(std::string::size_type len) {
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

  /*! Read a string from the specified location
   * \param p The position from where you want to read
   */
  std::string get_string(std::string::size_type len, size_type p) {
    rjump_to(p);
    return get_string(len);
  }

  /*! Flush the buffer */
  void flush() { fs.flush(); }

  /*! Close the file */
  void close() {
    fs.close();
    closed = true;
  }

  /*! Get the filename */
  std::string get_filename() const { return filename; }


  template <typename T> BinPtr<T> begin();  /*!< Returns the begin iterator */
  template <typename T> BinPtr<T> end();  /*!< Returns the end iterator */

 private:
  std::fstream fs;  /*!< The file stream */
  const std::string filename;  /*!< The filename */
  bool closed = false;  /*!< Tells if the file has been closed */
  std::shared_ptr<Bin> sptr;  /*!< A shared pointer which will point
                               * to the instance of the class itself
                               */
  bool opposite_endian;  /*!< Tells if the endianness you want to read/write
                          * is the opposite of the default one of the
			  * machine
			  */


  /*!
   * This function is used to handle the case when the user wants to
   * write multiple values by passing an initializer_list and doesn't
   * specify a casting type
   */
  template <typename K, typename T>
  void is_initializer_list_cast_specified(std::true_type, const std::initializer_list<T>& il) {
    write_many(std::begin(il), std::end(il));
  }


  /*!
   * This function is used to handle the case when the user wants to
   * write multiple values by passing an initializer_list and
   * specifies a casting type
   */
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


/*! An intermediate class used by the pointer class.
 * The purpose if the following class is to
 * treat differentiated BinPtr as they were
 * reference to lvalue, which they clearly
 * aren't
 */
template <typename T>
class TypeBin {
  template <typename K> friend void swap(TypeBin<K> &a, TypeBin<K> &b);
  template <typename K> friend class BinPtr;

 public:
  /*! The constructor
   * \param b The reference to the Bin instance
   * \param i The position of the file where to point
   */
  explicit TypeBin(Bin &b, std::streamsize i) : tmp_b(b), curr(i) { }
  
  /*! Getting a value from a differentiated pointer
   */
  operator T() & { return tmp_b.get_value<T>(curr); }
  
  /*! Setting a value to a differentiated pointer
   */
  void operator=(T a) & { tmp_b.template write<T>(a, curr); }

 private:
  Bin &tmp_b;  //!< The Bin instance which the iterator belongs to
  std::streamsize curr;  //!< The current position of the iterator

  /*!< Set the current position of the class in the file
   * \param i The position
   */
  void set_curr(std::streamsize i) { curr = i; }
};

/*! Swaps two "intermediate" iterators
 * \param a,b The "intermediate" iterators to be swapped
 */
template <typename T>
inline void swap(TypeBin<T> &a, TypeBin<T> &b) {
  T tmp = a.tmp_b.template get_value<T>(a.curr);
  a.tmp_b.template write<T>(b.tmp_b.template get_value<T>(b.curr), a.curr);
  b.tmp_b.template write<T>(tmp, b.curr);
}

/*! The actual pointer class
 *
 * +++++++++++++++ WARNING +++++++++++++++++
 * THE IMPLEMENTED ITERATOR IS EXTREMELY SLOWER.
 * IT IS STRONGLY DISCOURAGED ITS USE TO DEAL WITH
 * BIG FILES OR TO COMPUTE MANY OPERATIONS.
 * ITS USE IS RECOMMENDED FOR ELECANGE PURPOSE ONLY
 * +++++++++++++++++++++++++++++++++++++++++++++++
 */
template <typename T>
class BinPtr {
  template <typename K> friend typename std::iterator_traits<BinPtr<K>>::difference_type operator-(const BinPtr<K> &a, const BinPtr<K> &b);
  template <typename K> friend bool operator<(const BinPtr<K> &ptr1, const BinPtr<K> &ptr2);

 public:
  using size_type = Bin::size_type;
  using value_type = T;

  /*! Default constructor */
  BinPtr() : curr(0) { }

  /*! The main constructor
   * \param a The shared pointer to the Bin instance
   * \param sz The position where to point
   */
  explicit BinPtr(std::shared_ptr<Bin> &a, size_type sz = 0) : wptr(a), curr(sz), tb(*a, curr) { }

  /* The dereference operator
   * \return It returns the "intermediate" iterator which can be treated as an lvalue reference
   */
  TypeBin<T> &operator*() {
    auto p = check(0, "");
    p->wjump_to(curr);
    tb.set_curr(curr);
    return tb;
  }


  // Increment and decrement operators

  /*! Increment operator */
  BinPtr &operator++() {
    check(0, "");  // This step decreases the speed by about 30%. Unfortunately it's needed
    curr += sizeof(T);
    return *this;
  }

  /*! Decrement operator */
  BinPtr &operator--() {
    if (curr < static_cast<long int>(sizeof(T)))
      throw std::out_of_range("decrement past begin of Bin");
    curr -= sizeof(T);
    return *this;
  }

  /*! Increment operator */
  BinPtr operator++(int) {
    BinPtr ret = *this;
    ++*this;
    return ret;
  }

  /*! Decrement operator */
  BinPtr operator--(int) {
    BinPtr ret = *this;
    --*this;
    return ret;
  }

  /*! Random access iterator bahaviour */
  BinPtr operator+(size_type n) const {
    auto b = check(0, "");
    return BinPtr(b, curr + Bin::bytes<T>(n));
  }

  /*! Random access iterator bahaviour */
  BinPtr operator-(size_type n) const {
    auto b = check(0, "");
    return BinPtr(b, curr - Bin::bytes<T>(n));
  }

  // Relational operators

  /*! Equality operator */
  bool operator==(const BinPtr &wrb2) const {
    auto b1 = wptr.lock();
    auto b2 = wrb2.wptr.lock();
    if (!b1 || !b2)
      throw std::runtime_error("comparing invalid BinPtr(s)");
    return curr == wrb2.curr &&
                   std::addressof(b1->fs) == std::addressof(b2->fs);
  }

  /*! Inequality operator */
  bool operator!=(const BinPtr &wrb2) const { return !(*this == wrb2); }

  // Since this class handles built-in types, I
  // don't want to allow the -> operator.
  BinPtr operator->() const = delete;

 private:
  std::weak_ptr<Bin> wptr;  //!< A weak_ptr to the Bin instance
  size_type curr;  //!< The current poisition of the iterator
  TypeBin<T> tb;  //!< The "intermediate" iterator being handled

  /*! Performs various check given a position in the file.
   * It is used before yielding a pointer
   */
  std::shared_ptr<Bin> check(size_type i, const std::string &msg) const {
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

/*! Relational operator between two iterators */
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
