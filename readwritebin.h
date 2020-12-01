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

/*! \brief It handles a binary file for read/write operations
 */
class Bin {
  template <typename T> friend class BinPtr;
  template <typename T> friend class TypePtr;
  template <typename T> using iterator = BinPtr<T>;

 public:
  /*! \brief An empty class.
   *
   * This class is used as default type for
   * some template functions and it is used to understand
   * if the template parameter is omitted.
   */
  class TypeNotSpecified {};

  //! The type used to indicate positions inside the file
  using size_type = std::streamsize;

  /*! \brief The constructor.
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
    opposite_endian = use_little_endian != Bin::is_default_little_endian();
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

  /*! \brief Tells if the machine is little endian or big endian
   *
   * \return It returns a bool:\n 
   * \reutnr true:  The machine uses little endian\n 
   * \return false: The machine uses big endian
   */
  static bool is_default_little_endian() {
    short int n = 1;
    char *address = reinterpret_cast<char*>(&n);
    if (*(address + 1) == 1) return false;
    else if (*address == 1) return true;
    throw std::domain_error("Can't detect endianness of the machine!");
  }

  /*! \brief Compute the bytes occupied by instances of a given type
   * 
   * \tparam T The type used to determine the size of an instance of type T
   * \param n_instances The number of instances of an object of type T
   * \return It returns the number of bytes occupied by n_instances instances of T
   */
  template <typename T> static constexpr size_type bytes(size_type n_instances) {
    return sizeof(T) * n_instances;
  }

  /*! \brief Jump to a location in the file to read
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

  /*! \brief Jump to a location in the file to write
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

  /*! \brief Get the size of the file
   *
   * \return It returns the size of the file handled.
   */
  size_type size() {
    if (closed)
      throw std::domain_error("Can't tell size of closed file!");
    auto p = fs.tellp();
    fs.seekp(0, std::ios::end);
    auto sz = fs.tellp();
    fs.seekp(p);
    return sz;
  }
  
  /*! \brief Get the position you ar currently on (write)
   *
   * \param It returns the current position for writing.
   */
  size_type wpos() { return fs.tellp(); }

  /*! \brief Get the position you ar currently on (read)
   *
   * It seems to be identical to the write version
   * \param It returns the current position for reading. */
  size_type rpos() { return fs.tellg(); }

  /*! \brief Move by a certain number of steps, forward or backward.
   *
   * The size of the step is deduced by the type specified
   * \tparam T The type used to determine the size of a step
   * \param n_steps The number of steps
   */
  template <typename T = char>
  void wmove_by(std::streamoff n_steps) { fs.seekp(bytes<T>(n_steps), std::ios::cur); }

  /*! \brief Move by a certain number of steps, forward or backward.
   *
   * The size of the step is deduced by the type specified
   * \tparam T The type used to determine the size of a step
   * \param n_steps The number of steps
   */
  template <typename T = char>
  void rmove_by(std::streamoff n_steps) { fs.seekg(bytes<T>(n_steps), std::ios::cur); }

  /***********
   * WRITING *
   ***********/

  /*! \brief Write a value in the current position
   *
   * \tparam T
   * \parblock
   * The type of the input value. It is deduced from the
   * value assigned
   * \endparblock
   * \param val The value you want to write
   */
  template <typename T> void write(T val) {
    if (closed)
      throw std::domain_error("Can't write on closed file!");
    char *buf = reinterpret_cast<char*>(&val);
    if (opposite_endian) std::reverse(buf, buf + sizeof(T));
    fs.write(buf, sizeof(T));
  }

  /*! \brief Write multiple values starting from the current position
   *         given two iterators.
   *
   * \tparam T
   * \parblock
   * The type of the input iterators. It is deduced from the
   * iterators assigned.
   * \endparblock
   * \param begit The beginning interator
   * \param endit The ending interator
   */
  template <typename T> void write_many(T begit, T endit) {
    for (auto it = begit; it != endit; ++it)
      write(*it);
  }

  /*! \brief Write multiple values starting from the current position
   *         given two iterators.
   *
   * If you want, this implementation allows you to specify the
   * type you want to cast the values to
   * \tparam K The type used to interpret bytes of the output values
   * \tparam T
   * \parblock
   * The type of the input iterators. It is deduced from the
   * iterators assigned
   * \endparblock
   * \param begit The beginning interator
   * \param endit The ending interator
   */
  template <typename K, typename T> void write_many(T begit, T endit) {
    for (auto it = begit; it != endit; ++it)
      write<K>(*it);
  }

  /*! \brief Write multiple values starting from the current position
   *         given an initializer list.
   *
   * If you want, this implementation allows you to specify the
   * type you want to cast the values to
   * \tparam K
   * \parblock
   * The type used to interpret bytes of the output values. If
   * omitted it is used T by default
   * \endparblock
   * \tparam T
   * \parblock
   * The type of the input values. It is deduced from the
   * values assigned
   * \endparblock
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

  /*! \brief Write multiple values starting from the current position
   *         given a container.
   *
   * \tparam T
   * \parblock
   * The type of the container. It is deduced from the
   * container assigned. The type handled by the contained is
   * used to interpret bytes of the output values
   * \endparblock
   * \param vals The container
   */
  template <typename T>
  void write_many(const T &vals) {
    for (auto it = std::begin(vals); it != std::end(vals); ++it)
      write(*it);
  }


  /*! \brief Write multiple values starting from the current position
   *         given a container.
   *
   * If you want, this implementation allows you to specify the
   * type you want to cast the values to
   * \tparam K The type used to interpret bytes of the output values
   * \tparam T
   * \parblock
   * The type of the container. It is deduced from the
   * container assigned
   * \endparblock
   * \param vals The container
   */
  template <typename K, typename T>
  void write_many(const T &vals) {
    for (auto it = std::begin(vals); it != std::end(vals); ++it)
      write<K>(*it);
  }

  /**************************************
   * Specify the location where to write*
   **************************************/
  // Check the functions above for details
 

  /*! \brief Write a value in the specified position
   *
   * \tparam T
   * \parblock
   * The type of the input value. It is deduced from the
   * value assigned
   * \endparblock
   * \param val The value you want to write
   * \param p The position where you want to write
   */
  template <typename T> void write(T val, size_type p) {
    wjump_to(p);
    write(val);
  }


  /*! \brief Write multiple values starting from the specified position
   *         given an initializer list.
   *
   * If you want, this implementation allows you to specify the
   * type you want to cast the values to
   * \tparam K
   * \parblock
   * The type used to interpret bytes of the output values. If
   * omitted it is used T by default
   * \endparblock
   * \tparam T
   * \parblock
   * The type of the input values. It is deduced from the
   * values assigned
   * \endparblock
   * \param il The initializer list
   * \param p The position where you want to write
   */
  template <typename K = Bin::TypeNotSpecified, typename T> void write_many(const std::initializer_list<T> &il, size_type p) {
    wjump_to(p);
    write_many<K>(il);
  }


  /*! \brief Write multiple values starting from the specified position
   *         given two iterators.
   *
   * \tparam T
   * \parblock
   * The type of the input iterators. It is deduced from the
   * iterators assigned.
   * \endparblock
   * \param begit The beginning interator
   * \param endit The ending interator
   * \param p The position where you want to write
   */
  template <typename T> void write_many(T begit, T endit, size_type p) {
    wjump_to(p);
    write_many(begit, endit);
  }


  /*! \brief Write multiple values starting from the specified position
   *         given two iterators.
   *
   * If you want, this implementation allows you to specify the
   * type you want to cast the values to
   * \tparam K The type used to interpret bytes of the output values
   * \tparam T
   * \parblock
   * The type of the input iterators. It is deduced from the
   * iterators assigned
   * \endparblock
   * \param begit The beginning interator
   * \param endit The ending interator
   * \param p The position where you want to write
   */
  template <typename K, typename T> void write_many(T begit, T endit, size_type p) {
    wjump_to(p);
    write_many<K>(begit, endit);
  }


  /*! \brief Write multiple values starting from the current position
   *         given a container.
   *
   * \tparam T
   * \parblock
   * The type of the container. It is deduced from the
   * container assigned. The type handled by the contained is
   * used to interpret bytes of the output values
   * \endparblock
   * \param vals The container
   * \param p The position where you want to write
   */
  template <typename T>
  void write_many(const T &vals, size_type p) {
    wjump_to(p);
    for (auto it = std::begin(vals); it != std::end(vals); ++it)
      write(*it);
  }


  /*! \brief Write multiple values starting from the current position
   *         given a container.
   *
   * If you want, this implementation allows you to specify the
   * type you want to cast the values to
   * \tparam K The type used to interpret bytes of the output values
   * \tparam T
   * \parblock
   * The type of the container. It is deduced from the
   * container assigned
   * \endparblock
   * \param vals The container
   * \param p The position where you want to write
   */
  template <typename K, typename T>
  void write_many(const T &vals, size_type p) {
    wjump_to(p);
    for (auto it = std::begin(vals); it != std::end(vals); ++it)
      write<K>(*it);
  }

  /*! \brief Assign operator.
   *
   * It is used to write a in the file the value assigned
   * \tparam T
   * \parblock
   * The type used to interpret bytes. It is deduced from
   * the value assigned
   * \endparblock
   * \param val The value to write
   */
  template <typename T> void operator=(T val) { write(val); }

  /*! \brief Casting operator
   *
   * It is used to read from the file a value casted from
   * a certain type.
   * \tparam T The type used to interpret bytes
   */
  template <typename T> operator T() { return get_value<T>(); }

  /*! \brief Write a string in the current position
   *
   * \param s The string you want to write
   */
  void write_string(const std::string &s) {
    if (closed)
      throw std::domain_error("Can't write string on closed file!");
    fs.write(s.data(), bytes<char>(s.size()));
  }

  /*! \brief Write a string in the specified position
   *
   * \param s The string you want to write
   * \param p The position where you want to write
   */
  void write_string(const std::string &s, size_type p) {
    wjump_to(p);
    write_string(s);
  }

  /***********
   * READING *
   ***********/
  
  /*! \brief Read a single value of type T from the current position
   *
   * \tparam T The type used to interpret bytes
   * \return It returns the value read of type T
   */
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

  /*! \brief Read multiple values of type T from the current position
   *
   * \tparam T The type used to interpret bytes
   * \param n The number of elements of type T you want to read
   * \return It returns the values in a std::vector<T>
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


  /*! \brief Read a single value of type T from the specified position
   *
   * \tparam T The type used to interpret bytes
   * \param p The position from where you want to read
   * \return It returns the value read of type T
   */
  template <typename T = unsigned char> T get_value(size_type p) {
    rjump_to(p);
    return get_value<T>();
  }

  /*! \brief Read multiple values of type T from the specified position
   *
   * \tparam T The type used to interpret bytes
   * \param n The number of elements of type T you want to read
   * \param p The position from where you want to read
   * \return It returns the values in a std::vector<T>
   */
  template <typename T = unsigned char> std::vector<T> get_values(size_type n, size_type p) {
    rjump_to(p);
    return get_values<T>(n);
  }

  /*! \brief Read a string from the current location
   *
   * \param len The length of the string to read
   * \return It returns the string read
   */
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

  /*! \brief Read a string from the specified location
   *
   * \param len The length of the string to read
   * \param p The position from where you want to read
   * \return It returns the string read
   */
  std::string get_string(std::string::size_type len, size_type p) {
    rjump_to(p);
    return get_string(len);
  }

  /*! \brief Flush the buffer */
  void flush() { fs.flush(); }

  /*! \brief Close the file */
  void close() {
    fs.close();
    closed = true;
  }

  /*! \brief Get the filename
   *
   * \return It returns the file name
   */
  std::string get_filename() const { return filename; }


  template <typename T> BinPtr<T> begin();
  template <typename T> BinPtr<T> end();

 private:
  std::fstream fs;  /*!< \brief The file stream */
  const std::string filename;  /*!< \brief The file name */
  bool closed = false;  /*!< \brief Tells if the file has been closed */
  std::shared_ptr<Bin> sptr;  /*!< \brief A shared pointer which will point
                               *          to the instance of the class itself
                               */
  bool opposite_endian;  /*!< \brief Tells if the endianness you want to read/write
                          *          is the opposite of the default one of the machine
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


/*! \brief A class handled by PtrBin
 *
 * An intermediate class used by the pointer class PtrBin.
 * The purpose if the following class is to
 * treat differentiated BinPtr as they were
 * reference to lvalue, which they clearly
 * aren't
 * \tparam T The type handled by the pointer
 */
template <typename T>
class TypeBin {
  template <typename K> friend void swap(TypeBin<K> &a, TypeBin<K> &b);
  template <typename K> friend class BinPtr;

 public:
  /*! \brief The constructor
   *
   * \param b The reference to the Bin instance
   * \param i The position of the file where to point
   */
  explicit TypeBin(Bin &b, std::streamsize i) : tmp_b(b), curr(i) { }
  
  /*! \brief Getting a value from a differentiated pointer
   */
  operator T() & { return tmp_b.get_value<T>(curr); }
  
  /*! \brief Setting a value to a differentiated pointer
   *
   * \param a The value assigned to the differentiated pointer
   */
  void operator=(T a) & { tmp_b.template write<T>(a, curr); }

 private:
  Bin &tmp_b;  //!< \brief The Bin instance which the iterator belongs to
  std::streamsize curr;  //!< \brief The current position of the iterator

  /*!< \brief Set the current position of the class in the file
   *
   * \param i The position
   */
  void set_curr(std::streamsize i) { curr = i; }
};

/*! \brief Swaps two "intermediate" iterators
 *
 * \tparam T The type handled by the "intermediate" iterators to be swapped
 * \param a,b The "intermediate" iterators to be swapped
 */
template <typename T>
inline void swap(TypeBin<T> &a, TypeBin<T> &b) {
  T tmp = a.tmp_b.template get_value<T>(a.curr);
  a.tmp_b.template write<T>(b.tmp_b.template get_value<T>(b.curr), a.curr);
  b.tmp_b.template write<T>(tmp, b.curr);
}

/*! \brief The actual pointer class
 *
 * +++++++++++++++ WARNING +++++++++++++++++
 * THE IMPLEMENTED ITERATOR IS EXTREMELY SLOWER.
 * IT IS STRONGLY DISCOURAGED ITS USE TO DEAL WITH
 * BIG FILES OR TO COMPUTE MANY OPERATIONS.
 * ITS USE IS RECOMMENDED FOR ELECANGE PURPOSE ONLY
 * +++++++++++++++++++++++++++++++++++++++++++++++
 *
 * \tparam T The type handled by the pointer
 */
template <typename T>
class BinPtr {
  template <typename K> friend typename std::iterator_traits<BinPtr<K>>::difference_type operator-(const BinPtr<K> &a, const BinPtr<K> &b);
  template <typename K> friend bool operator<(const BinPtr<K> &ptr1, const BinPtr<K> &ptr2);

 public:
  using size_type = Bin::size_type;
  using value_type = T;

  /*! \brief Default constructor */
  BinPtr() : curr(0) { }

  /*! \brief The main constructor
   *
   * \param a The shared pointer to the Bin instance
   * \param sz The position where to point
   */
  explicit BinPtr(std::shared_ptr<Bin> &a, size_type sz = 0) : wptr(a), curr(sz), tb(*a, curr) { }

  /* \brief The dereference operator
   *
   * \return It returns the "intermediate" iterator which can be treated as an lvalue
   * \return reference, both for reading and writing
   */
  TypeBin<T> &operator*() {
    auto p = check(0, "");
    p->wjump_to(curr);
    tb.set_curr(curr);
    return tb;
  }


  // Increment and decrement operators

  /*! \brief Increment operator
   *
   * \return It returns the increased iterator
   */
  BinPtr &operator++() {
    check(0, "");  // This step decreases the speed by about 30%. Unfortunately it's needed
    curr += sizeof(T);
    return *this;
  }

  /*! \brief Decrement operator
   *
   * \return It returns the decreased iterator
   */
  BinPtr &operator--() {
    if (curr < static_cast<long int>(sizeof(T)))
      throw std::out_of_range("decrement past begin of Bin");
    curr -= sizeof(T);
    return *this;
  }

  /*! \brief Increment operator
   *
   * \return It returns the iterator before being increased
   */
  BinPtr operator++(int) {
    BinPtr ret = *this;
    ++*this;
    return ret;
  }

  /*! \brief Decrement operator
   *
   * \return It returns the iterator before being decreased
   */
  BinPtr operator--(int) {
    BinPtr ret = *this;
    --*this;
    return ret;
  }

  /*! \brief Random access iterator bahaviour
   *
   * \param n The number of forward steps of the iterator
   * \return It returns the increased iterator
   */
  BinPtr operator+(size_type n) const {
    auto b = check(0, "");
    return BinPtr(b, curr + Bin::bytes<T>(n));
  }

  /*! \brief Random access iterator bahaviour
   *
   * \param n The number of backward steps of the iterator
   * \return It returns the decreased iterator
   */
  BinPtr operator-(size_type n) const {
    auto b = check(0, "");
    return BinPtr(b, curr - Bin::bytes<T>(n));
  }

  // Relational operators

  /*! \brief Equality operator
   *
   * \param wrb2 The pointer on the right side of the operator
   */
  bool operator==(const BinPtr &wrb2) const {
    auto b1 = wptr.lock();
    auto b2 = wrb2.wptr.lock();
    if (!b1 || !b2)
      throw std::runtime_error("comparing invalid BinPtr(s)");
    return curr == wrb2.curr &&
                   std::addressof(b1->fs) == std::addressof(b2->fs);
  }

  /*! \brief Inequality operator
   *
   * \param wrb2 The pointer on the right side of the operator
   */
  bool operator!=(const BinPtr &wrb2) const { return !(*this == wrb2); }


  /*! \brief Deleted arrow operator
   *
   * Since this class handles built-in types, I
   * don't want to allow the -> operator.
   */
  BinPtr operator->() const = delete;

 private:
  std::weak_ptr<Bin> wptr;  //!< \brief A weak_ptr to the Bin instance
  size_type curr;  //!< \brief The current poisition of the iterator
  TypeBin<T> tb;  //!< \brief The "intermediate" iterator being handled

  /*! \brief Performs various check given a position in the file.
   *
   * It is used before yielding a pointer
   * \param i The point of the file to check
   * \param msg The error message to throw if the point is out of bounds.
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

/*! \brief Return the being iterator
 *
 * \tparam T The type that will be handled by the iterator
 * \return It returns the begin iterator
 */
template <typename T>
BinPtr<T> Bin::begin() { return BinPtr<T>(sptr); }


/*! \brief Return the end iterator
 *
 * \tparam T The type that will be handled by the iterator
 * \return It returns the end iterator
 */
template <typename T>
BinPtr<T> Bin::end() { return BinPtr<T>(sptr, size()); }

/*! \brief Relational operator between two iterators
 *
 * \param ptr1,ptr2 The pointers to be compared
 */
template <typename T>
bool operator<(const BinPtr<T> &ptr1, const BinPtr<T> &ptr2) {
  return ptr1.curr < ptr2.curr;
}

/*! \brief Difference between two iterators
 *
 * \param a The left-hand side iterator
 * \param b The right-hand side iterator
 * \return It returns the distance between the two iterators
 */
template <typename T>
typename std::iterator_traits<BinPtr<T>>::difference_type operator-(const BinPtr<T> &a, const BinPtr<T> &b) {
  return (a.curr - b.curr) / sizeof(T);
}


namespace std {
    /*! This "iterator_traits" part has been written with the only
     * goal to make the code work, being aware that this could further affect
     * negatively its already lacking cleanliness. For example it allows to
     * use BinPtr for functions of <algorithm>
     * \tparam T The type handled by the pointer
     */
    template <typename T>
    struct iterator_traits<BinPtr<T>> {
      typedef ptrdiff_t difference_type;
      typedef TypeBin<T>&& value_type;
      typedef TypeBin<T>&& reference;  //!< \brief In general it whould be T&
      typedef BinPtr<T> pointer;  //!< \brief In general it should be T*
      typedef std::random_access_iterator_tag iterator_category;
    };
}

#endif // READWRITEBIN_H
