#ifndef READWRITEBIN_H
#define READWRITEBIN_H

// Questa libreria dovrebbe permettere
// di semplificare la lettura e la
// scrittura di file in binario

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

template <typename T> class ReadBinPtr;
template <typename T> class WriteBinPtr;

class ReadBin {
  template <typename T> friend class ReadBinPtr;
  template <typename T> using iterator = ReadBinPtr<T>;
  using size_type = std::streamsize;

 public:
  // Il motivo per cui passo come destructor allo shared_ptr
  // semplicemente una funzione che mette a 0 il puntatore
  // è che la classe stessa è racchiusa in quel puntatore,
  // per cui quando viene chiamato il destructor della
  // classe, quando tocca all' shared_ptr, questo non
  // cerca di distruggere il contenuto (chiamando
  // nuovamente il destructor della classe), ma lo
  // mette = 0 e la classe può continuare il processo
  // di distruzione evitando che il destructor venga
  // chiamato più di una volta. Omettendo questo
  // particolare durante la distruzione dà errore
  explicit ReadBin(const std::string &filename, bool is_little_endian = true) : fis(filename, std::ios::binary), little_endian(is_little_endian), sptr(this, [] (ReadBin *p) { return p = 0; }) {
    if (!fis.good() || !fis.is_open())
      throw std::domain_error("Couldn't open file!");
    fis.seekg(0, std::ios::end);
    sz = fis.tellg();
    fis.seekg(0, std::ios::beg);  // Anche fis.seekg(0); va bene
  }

  void jump_to(std::streampos point) {
    if (closed)
      throw std::domain_error("Can't operate on closed file!");
    if (point > sz)
      throw std::domain_error("Can't go past EOF!");
    fis.seekg(point);
  }

  void move_by(std::streamoff n) { fis.seekg(n, std::ios::cur); }

  // Setters
  void set_little_endian(bool is_it) { little_endian = is_it; }

  // Getters
  size_type size() const {
    if (closed)
      throw std::domain_error("Can't tell size of closed file!");
    return sz;
  }

  size_type pos() const { return fis.tellg(); }

  // Lettura dei byte del file
  template <typename T> T get_value() const {
    if (closed)
      throw std::domain_error("Can't read from closed file!");
    char buf[sizeof(T)];
    fis.read(buf, sizeof(T));
    // Credo che per i tipi float non cambi nulla little o big endian
    if (!little_endian && typeid(T) != typeid(double) && typeid(T) != typeid(float))
      std::reverse(&buf[0], &buf[sizeof(T)]);
    T *d = reinterpret_cast<T*>(buf);
    return *d;
  }

  template <typename T> T get_value(size_type n) {
    jump_to(n);
    return get_value<T>();
  }

  std::string get_string(std::string::size_type len) const {
    if (closed)
      throw std::domain_error("Can't read from closed file!");
    if (len > static_cast<std::string::size_type>(sz - fis.tellg()))
      throw std::domain_error("Can't read past EOF!");

    char *buf = new char[len + 1];
    fis.read(buf, len);
    buf[len] = '\0';
    std::string ret = buf;
    delete[] buf;
    return ret;
  }

  void close() {
    fis.close();
    closed = true;
  }

  template <typename T> ReadBinPtr<T> begin();
  template <typename T> ReadBinPtr<T> end();

 private:
  mutable std::ifstream fis;
  bool little_endian;
  size_type sz;
  bool closed = false;
  std::shared_ptr<ReadBin> sptr;
};

class WriteBin {
  template <typename T> friend class WriteBinPtr;
  template <typename T> using iterator = WriteBinPtr<T>;
  using size_type = std::streamsize;

 public:
  explicit WriteBin(const std::string &filename, bool truncate = false, bool is_little_endian = true) :
      little_endian(is_little_endian), sptr(this, [] (WriteBin *p) { return p = 0; }) {
    if (truncate)
      // L'unico modo per far funzionare seekp, ovvero jump_to,
      // è aprire il file con ios::app, ios::in e ios::out insieme,
      // se no niente. Severamente vietato ios::app, in quel modo
      // proprio non funziona seekp.
      // Il file lo apro dentro il corpo e non nella initializer list
      // perché per qualche motivo non mi prendeva l'operatore "?:"
      fos.open(filename,
               std::ios::out |
               std::ios::in |
               std::ios::ate | std::ios::trunc);
    else
      fos.open(filename,
               std::ios::out |
               std::ios::in |
               std::ios::ate);
    if (!fos.good() || !fos.is_open())
      throw std::domain_error("Couldn't open file!");
  }

  void jump_to(std::streampos point) {
    if (closed)
      throw std::domain_error("Can't operate on closed file!");
    if (point > size())
      throw std::domain_error("Can't go past EOF!");
    fos.seekp(point);
  }

  // Getters
  size_type size() {
    if (closed)
      throw std::domain_error("Can't tell size of closed file!");
    auto p = fos.tellp();
    fos.seekp(0, std::ios::end);
    auto sz = fos.tellp();
    fos.seekp(p);
    return sz;
  }
  size_type pos() { return fos.tellp(); }

  void move_by(std::streamoff n) { fos.seekp(n, std::ios::cur); }

  template <typename T> void write(T v) {
    if (closed)
      throw std::domain_error("Can't write on closed file!");
    fos.write(reinterpret_cast<char*>(&v), sizeof(T));
  }

  template <typename T> void write(T v, size_type n) {
    jump_to(n);
    write(v);
  }

  template <typename T> void operator=(T v) { write(v); }

  void write_string(const std::string &v) {
    if (closed)
      throw std::domain_error("Can't write on closed file!");
    fos.write(v.data(), sizeof(char) * v.size());
  }


  void close() {
    fos.close();
    closed = true;
  }

  template <typename T> WriteBinPtr<T> begin();
  template <typename T> WriteBinPtr<T> end();

 private:
  std::ofstream fos;
  bool little_endian;  // Non lo ho implementato il big endian :(
  bool closed = false;
  std::shared_ptr<WriteBin> sptr;
};

/********************* ITERATORI **********************/

template <typename T>
class ReadBinPtr {
  template <typename K> friend typename std::iterator_traits<ReadBinPtr<K>>::difference_type operator-(const ReadBinPtr<K> &a, const ReadBinPtr<K> &b);
 public:
  using size_type = ReadBin::size_type;
  using value_type = T;
  // typedef typename T value_type;

  ReadBinPtr() : curr(0) { }
  explicit ReadBinPtr(std::shared_ptr<ReadBin> &a, size_type sz = 0) : wptr(a), curr(sz) { }

  // E' solo lettura, non voglio restituire nessuna reference
  // (anche perché non parei come farlo)
  T operator*() const {
    auto b = check(curr, "dereference past end");
    return b->template get_value<T>(curr);  // sta cosa è nuova
  }

  // Increment and decrement operators
  ReadBinPtr &operator++() {
    check(curr + sizeof(T), "increment past end of ReadBin");
    curr += sizeof(T);
    return *this;
  }

  ReadBinPtr &operator--() {
    curr -= sizeof(T);
    check(curr, "increment past begin of ReadBin");
    return *this;
  }

  ReadBinPtr operator++(int) {
    ReadBinPtr ret = *this;
    ++*this;
    return ret;
  }

  ReadBinPtr operator--(int) {
    ReadBinPtr ret = *this;
    --*this;
    return ret;
  }

  ReadBinPtr operator+(size_type n) const {
    auto b = check(curr + sizeof(T) * n, "iterator past end of ReadBin");
    return ReadBinPtr(b, curr + sizeof(T) * n);
  }

  ReadBinPtr operator-(size_type n) const {
    auto b = check(curr - sizeof(T) * n, "iterator past begin of ReadBin");
    return ReadBinPtr(b, curr - sizeof(T) * n);
  }

  // Relational operators
  bool operator==(const ReadBinPtr &rb2) const {
    auto b1 = wptr.lock();
    auto b2 = rb2.wptr.lock();
    if (!b1 || !b2)
      throw std::runtime_error("something wrong while comparing ReadBinPtr");
    // Uso std::addressof() per evitare di usare eventualmente
    // l'"operatore &" nel caso un giorno mi venga di overloadarlo
    return curr == rb2.curr &&
                   std::addressof(b1->fis) == std::addressof(b2->fis);
  }

  bool operator!=(const ReadBinPtr &rb2) const { return !(*this == rb2); }

  // Faccio finta di puntare a dei valori
  // built-in, non ha senso assegnargli
  // questo operatore
  ReadBinPtr operator->() const = delete;

 private:
  std::weak_ptr<ReadBin> wptr;
  size_type curr;

  std::shared_ptr<ReadBin> check(size_type i, const std::string &msg) const {
    auto ret = wptr.lock();
    if (!ret)
      throw std::runtime_error("Unbound ReadBin");
    if (ret->closed)
      throw std::runtime_error("The file was closed!");
    if (i > ret->size())
      throw std::out_of_range(msg);
    return ret;
  }
};

template <typename T>
ReadBinPtr<T> ReadBin::begin() { return ReadBinPtr<T>(sptr); }

template <typename T>
ReadBinPtr<T> ReadBin::end() { return ReadBinPtr<T>(sptr, size()); }

template <typename T>
class WriteBinPtr {
  template <typename K> friend typename std::iterator_traits<WriteBinPtr<K>>::difference_type operator-(const WriteBinPtr<K> &a, const WriteBinPtr<K> &b);

 public:
  using size_type = WriteBin::size_type;
  using value_type = T;
  // typedef typename T value_type;

  WriteBinPtr() : curr(0) { }
  explicit WriteBinPtr(std::shared_ptr<WriteBin> &a, size_type sz = 0) : wptr(a), curr(sz) { }

  // E' solo lettura, non voglio restituire nessuna reference
  // (anche perché non parei come farlo)
  WriteBin &operator*() {
    auto p = check(curr, "something's wrong while dereferencing WriteBinPtr");
    p->jump_to(curr);
    return *p;
  }

  // Increment and decrement operators
  WriteBinPtr &operator++() {
    check(curr + sizeof(T), "increment past end of WriteBin");
    curr += sizeof(T);
    return *this;
  }

  WriteBinPtr &operator--() {
    curr -= sizeof(T);
    check(curr, "increment past begin of WriteBin");
    return *this;
  }

  WriteBinPtr operator++(int) {
    WriteBinPtr ret = *this;
    ++*this;
    return ret;
  }

  WriteBinPtr operator--(int) {
    WriteBinPtr ret = *this;
    --*this;
    return ret;
  }

  WriteBinPtr operator+(size_type n) const {
    auto b = check(curr + sizeof(T) * n, "iterator past end of WriteBin");
    return WriteBinPtr(b, curr + sizeof(T) * n);
  }

  WriteBinPtr operator-(size_type n) const {
    auto b = check(curr - sizeof(T) * n, "iterator past begin of WriteBin");
    return WriteBinPtr(b, curr - sizeof(T) * n);
  }

  // Relational operators
  bool operator==(const WriteBinPtr &rb2) const {
    auto b1 = wptr.lock();
    auto b2 = rb2.wptr.lock();
    if (!b1 || !b2)
      throw std::runtime_error("something wrong while comparing WriteBinPtr");
    // Uso std::addressof() per evitare di usare eventualmente
    // l'"operatore &" nel caso un giorno mi venga di overloadarlo
    return curr == rb2.curr &&
                   std::addressof(b1->fos) == std::addressof(b2->fos);
  }

  bool operator!=(const WriteBinPtr &rb2) const { return !(*this == rb2); }

  // Faccio finta di puntare a dei valori
  // built-in, non ha senso assegnargli
  // questo operatore
  WriteBinPtr operator->() const = delete;

 private:
  std::weak_ptr<WriteBin> wptr;
  size_type curr;

  std::shared_ptr<WriteBin> check(size_type i, const std::string &msg) const {
    auto ret = wptr.lock();
    if (!ret)
      throw std::runtime_error("Unbound WriteBin");
    if (ret->closed)
      throw std::runtime_error("The file was closed!");
    if (i > ret->size())
      throw std::out_of_range(msg);
    return ret;
  }
};

template <typename T>
WriteBinPtr<T> WriteBin::begin() { return WriteBinPtr<T>(sptr); }

template <typename T>
WriteBinPtr<T> WriteBin::end() { return WriteBinPtr<T>(sptr, size()); }

template <typename T>
typename std::iterator_traits<ReadBinPtr<T>>::difference_type operator-(const ReadBinPtr<T> &a, const ReadBinPtr<T> &b) {
  return (a.curr - b.curr) / sizeof(T);
}

template <typename T>
typename std::iterator_traits<WriteBinPtr<T>>::difference_type operator-(const WriteBinPtr<T> &a, const WriteBinPtr<T> &b) {
  return (a.curr - b.curr) / sizeof(T);
}
namespace std {
    template <typename T>
    struct iterator_traits<ReadBinPtr<T>> {
        typedef ptrdiff_t difference_type;
        typedef T value_type;
        typedef T reference;  // poi vedo meglio se va bene, in genere è T&
        typedef ReadBinPtr<T> pointer;  // in genere T*
        typedef std::input_iterator_tag iterator_category;
    };
}
namespace std {
    template <typename T>
    struct iterator_traits<WriteBinPtr<T>> {
      typedef ptrdiff_t difference_type;
      typedef WriteBin value_type;
      typedef WriteBin reference;  // poi vedo meglio se va bene, in genere è T&
      typedef ReadBinPtr<T> pointer;  // in genere T*
      typedef std::output_iterator_tag iterator_category;
    };
}

// ************************************************************
// *                                                          *
// *                                                          *
// *                                                          *
// *                                                          *
// *                                                          *
// *                                                          *
// *            Read e write contemporaneamente               *
// *                                                          *
// *                                                          *
// *                                                          *
// *                                                          *
// *                                                          *
// *                                                          *
// ************************************************************

template <typename T> class BinPtr;
template <typename T> class TypeBin;

class Bin {
  template <typename T> friend class BinPtr;
  template <typename T> friend class TypePtr;
  template <typename T> using iterator = BinPtr<T>;

 public:
  using size_type = std::streamsize;
  // Il motivo per cui passo come destructor allo shared_ptr
  // semplicemente una funzione che mette a 0 il puntatore
  // è che la classe stessa è racchiusa in quel puntatore,
  // per cui quando viene chiamato il destructor della
  // classe, quando tocca all' shared_ptr, questo non
  // cerca di distruggere il contenuto (chiamando
  // nuovamente il destructor della classe), ma lo
  // mette = 0 e la classe può continuare il processo
  // di distruzione evitando che il destructor venga
  // chiamato più di una volta. Omettendo questo
  // particolare durante la distruzione dà errore
  explicit Bin(const std::string &filename, bool truncate = false, bool is_little_endian = true) :
      little_endian(is_little_endian), sptr(this, [] (Bin *p) { return p = 0; }) {
    if (truncate)
      // L'unico modo per far funzionare seekp, ovvero jump_to,
      // è aprire il file con ios::app, ios::in e ios::out insieme,
      // se no niente. Severamente vietato ios::app, in quel modo
      // proprio non funziona seekp.
      // Il file lo apro dentro il corpo e non nella initializer list
      // perché per qualche motivo non mi prendeva l'operatore "?:"
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
    if (closed)
      throw std::domain_error("Can't jump and read closed file!");
    if (point > size())
      throw std::domain_error("Can't jump and read past EOF!");
    fs.seekg(point);
  }

  void wjump_to(std::streampos point) {
    if (closed)
      throw std::domain_error("Can't jump and write on closed file!");
    fs.seekp(point);
  }

  // Getters
  size_type size() {
    if (closed)
      throw std::domain_error("Can't tell size of closed file!");
    auto p = fs.tellp();
    fs.seekp(0, std::ios::end);
    auto sz = fs.tellp();
    fs.seekp(p);
    return sz;
  }
  size_type wpos() { return fs.tellp(); }
  size_type rpos() { return fs.tellg(); }

  template <typename T = char>
  void wmove_by(std::streamoff n) { fs.seekp(n * sizeof(T), std::ios::cur); }
  template <typename T = char>
  void rmove_by(std::streamoff n) { fs.seekg(n * sizeof(T), std::ios::cur); }

  /**************************
   * PARTE IN CUI SI SCRIVE *
   **************************/
  template <typename T> void write(T v) {
    if (closed)
      throw std::domain_error("Can't write on closed file!");
    char *buf = reinterpret_cast<char*>(&v);
    if (!little_endian) std::reverse(buf, buf + sizeof(T));
    fs.write(buf, sizeof(T));
  }

  template <typename T> void write_many(T beg, T end) {
    for (auto it = beg; it != end; ++it)
      write(*it);
  }

  // Nel caso uno voglia specificare come castare gli elementi
  template <typename K, typename T> void write_many(T beg, T end) {
    for (auto it = beg; it != end; ++it)
      write<K>(*it);
  }

  template <typename T> void write_many(const std::initializer_list<T> &il) {
    write_many(std::begin(il), std::end(il));
  }

  // Nel caso uno voglia specificare come castare gli elementi
  template <typename K, typename T> void write_many(const std::initializer_list<T> &il) {
    write_many<K>(std::begin(il), std::end(il));
  }

  template <typename T>
  void write_many(const T &v) {
    for (auto it = std::begin(v); it != std::end(v); ++it)
      write(*it);
  }

  // Nel caso uno voglia specificare come castare gli elementi
  template <typename K, typename T>
  void write_many(const T &v) {
    for (auto it = std::begin(v); it != std::end(v); ++it)
      write<K>(*it);
  }

  template <typename T> void write(T v, size_type p) {
    wjump_to(p);
    write(v);
  }

  template <typename T> void write_many(const std::initializer_list<T> &v, size_type p) {
    wjump_to(p);
    write_many(v);
  }

  // Nel caso uno voglia specificare come castare gli elementi
  template <typename K, typename T> void write_many(const std::initializer_list<T> &v, size_type p) {
    wjump_to(p);
    write_many<K>(v);
  }

  template <typename T> void write_many(T beg, T end, size_type p) {
    wjump_to(p);
    write_many(beg, end);
  }

  // Nel caso uno voglia specificare come castare gli elementi
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

  // Nel caso uno voglia specificare come castare gli elementi
  template <typename K, typename T>
  void write_many(const T &v, size_type p) {
    wjump_to(p);
    for (auto it = std::begin(v); it != std::end(v); ++it)
      write<K>(*it);
  }

  template <typename T> void operator=(T v) { write(v); }
  template <typename T> operator T() { return get_value<T>(); }

  void write_string(const std::string &v) {
    if (closed)
      throw std::domain_error("Can't write string on closed file!");
    fs.write(v.data(), sizeof(char) * v.size());
  }
  void write_string(const std::string &v, size_type p) {
    wjump_to(p);
    write_string(v);
  }

  /*************************
   * PARTE IN CUI SI LEGGE *
   *************************/
  // Lettura dei byte del file
  template <typename T = unsigned char> T get_value() {
    if (closed)
      throw std::domain_error("Can't read from closed file!");
    if (static_cast<decltype(sizeof(T))>(size() - rpos()) < sizeof(T))
      throw std::runtime_error("Trying to read past EOF!");
    char buf[sizeof(T)];
    fs.read(buf, sizeof(T));
    // Credo che per i tipi float non cambi nulla little o big endian
    if (!little_endian && typeid(T) != typeid(double) && typeid(T) != typeid(float))
      std::reverse(&buf[0], &buf[sizeof(T)]);
    T *d = reinterpret_cast<T*>(buf);
    return *d;
  }

  template <typename T = unsigned char> std::vector<T> get_values(size_type n) {
    if (closed)
      throw std::domain_error("Can't write on closed file!");
    if (static_cast<decltype(sizeof(T))>(size() - rpos()) < sizeof(T) * n)
      throw std::runtime_error("Trying to read past EOF!");
    char *buf = new char[sizeof(T) * n];
    fs.read(buf, sizeof(T) * n);
    std::vector<T> ret(n);

    if (!little_endian && typeid(T) != typeid(double) && typeid(T) != typeid(float)) {
      for (int i = 0; i != n; ++i)
        std::reverse(&buf[i * sizeof(T)], &buf[(i + 1) * sizeof(T)]);
    }
    for (int i = 0; i != n; ++i)
      ret[i] = *reinterpret_cast<T*>(buf + (i * sizeof(T)));
    return ret;
  }

  template <typename T = unsigned char> T get_value(size_type p) {
    rjump_to(p);
    return get_value<T>();
  }

  template <typename T = unsigned char> std::vector<T> get_values(size_type n, size_type p) {
    rjump_to(p);
    return get_values<T>(n);
  }

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

  std::string get_string(std::string::size_type len, size_type p) {
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
};

/*************** ITERATORE *******************/
// PER SCRIVERE FILE GRANDI EVITARE L'ITERATORE
// CHE E' MOLTO LENTO. DA USARE SOLO PER
// COMODITA'/ELEGANZA

// QUESTA CLASSE SERVE COME PASSAGGIO INTERMEDIO
// PER TRATTARE I BinPtr DEREFERENZIATI COME
// SE FOSSERO DELLE REFERENCE A LVALUE
template <typename T>
class TypeBin {
  template <typename K> friend void swap(TypeBin<K> &a, TypeBin<K> &b);
  template <typename K> friend class BinPtr;

 public:
  explicit TypeBin(Bin &b, std::streamsize i) : tmp_b(b), curr(i) { }

  operator T() & { return tmp_b.get_value<T>(curr); }
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

template <typename T>
class BinPtr {
  template <typename K> friend typename std::iterator_traits<BinPtr<K>>::difference_type operator-(const BinPtr<K> &a, const BinPtr<K> &b);
template <typename K> friend bool operator<(const BinPtr<K> &ptr1, const BinPtr<K> &ptr2);

 public:
  using size_type = Bin::size_type;
  using value_type = T;

  BinPtr() : curr(0) { }
  explicit BinPtr(std::shared_ptr<Bin> &a, size_type sz = 0) : wptr(a), curr(sz), tb(*a, curr) { }

  // IN REALTA' VORREI CHE RETURNASSE TypeBin<T> &&
  TypeBin<T> &operator*() {
    // Visto che magari voglio scrivere più avanti, non
    // voglio fare controlli sulla dimensione, ma solo
    // sull'esistenza
    auto p = check(0, "");
    p->wjump_to(curr);
    tb.set_curr(curr);
    return tb;
  }


  // Increment and decrement operators
  BinPtr &operator++() {
    // Visto che magari voglio scrivere più avanti, non
    // voglio fare controlli sulla dimensione, ma solo
    // sull'esistenza
    check(0, "");  // Questo fa perdere circa il 30% di velocità
    curr += sizeof(T);
    return *this;
  }

  BinPtr &operator--() {
    // Visto che magari stavo molto avanti rispetto
    // alla fine e voglio poter andare indietro di 1
    // stando ancora oltre la fine per poi scrivere
    // controllo solo che non decrementi prima dello
    // inizio
    if (curr < sizeof(T))
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
    // Uso std::addressof() per evitare di usare eventualmente
    // l'"operatore &" nel caso un giorno mi venga di overloadarlo
    return curr == wrb2.curr &&
                   std::addressof(b1->fs) == std::addressof(b2->fs);
  }

  bool operator!=(const BinPtr &wrb2) const { return !(*this == wrb2); }

  // Faccio finta di puntare a dei valori
  // built-in, non ha senso assegnargli
  // questo operatore
  BinPtr operator->() const = delete;

 private:
  std::weak_ptr<Bin> wptr;
  size_type curr;
  TypeBin<T> tb;

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

template <typename T>
bool operator<(const BinPtr<T> &ptr1, const BinPtr<T> &ptr2) {
  return ptr1.curr < ptr2.curr;
}

template <typename T>
typename std::iterator_traits<BinPtr<T>>::difference_type operator-(const BinPtr<T> &a, const BinPtr<T> &b) {
  return (a.curr - b.curr) / sizeof(T);
}

namespace std {
    template <typename T>
    struct iterator_traits<BinPtr<T>> {
      typedef ptrdiff_t difference_type;
      typedef TypeBin<T>&& value_type;
      typedef TypeBin<T>&& reference;  // poi vedo meglio se va bene, in genere è T&
      typedef BinPtr<T> pointer;  // in genere T*
      typedef std::random_access_iterator_tag iterator_category;
    };
}

#endif // READWRITEBIN_H
