#ifndef QT_EXT_H
#define QT_EXT_H

/**
  * @brief Disable copy class instance including move.
  * Extends Qt's `Q_DISABLE_COPY`
  */
#ifndef Q_DISABLE_COPY_X
#define Q_DISABLE_COPY_X(Class) \
   Q_DISABLE_COPY(Class) \
   Class(Class &&) Q_DECL_NOEXCEPT Q_DECL_EQ_DELETE; \
   Class &operator=(Class &&) Q_DECL_NOEXCEPT Q_DECL_EQ_DELETE;
#endif // Q_DISABLE_COPY_X

#endif // QT_EXT_H
