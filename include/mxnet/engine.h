/*!
 * Copyright (c) 2015 by Contributors
 * \file engine.h
 * \brief Engine that schedules all the operations according to dependency.
 */
#ifndef MXNET_ENGINE_H_
#define MXNET_ENGINE_H_

#include <dmlc/base.h>
#if DMLC_USE_CXX11
#include <memory>
#include <functional>
#endif
#include <vector>
#include "./base.h"

namespace mxnet {
/*! \brief namespace of engine internal types. */
namespace engine {
/*! \brief Internal representation of variable. */
struct Var;
/*! \brief Internal representation of operator.  */
struct Opr;
/*! \brief Variable pointer type, usually hold by user used to specify dependencies. */
typedef Var* VarHandle;
/*! \brief Operator pointer type, usually hold by user.*/
typedef Opr* OprHandle;
}  // namespace engine

#if DMLC_USE_CXX11
/*! \brief Function property, used to hint what action is pushed to engine. */
enum class FnProperty {
  /*! \brief Normal operation */
  kNormal,
  /*! \brief Copy operation from GPU to other devices */
  kCopyFromGPU,
  /*! \brief Copy operation from CPU to other devices */
  kCopyToGPU,
  /*! \brief Asynchronous function call */
  kAsync
};  // enum class FnProperty

/*!
 * \brief Dependency engine that schedules operations.
*/
class Engine {
 public:
  /*!
   * \brief OnComplete Callback to the engine,
   *  called by AsyncFn when action completes
   */
  class CallbackOnComplete {
   public:
    // use implicit copy and assign
    /*! \brief involve the callback */
    inline void operator()() const {
      (*callback_)(engine_, param_);
    }

   private:
    /*! \brief engine can see content of callback */
    friend class ::mxnet::Engine;
    /*! \brief the real callback */
    void (*callback_)(Engine *, void *);
    /*! \brief the engine class passed to callback */
    Engine* engine_;
    /*! \brief the parameter set on callback */
    void* param_;
  };
  /*! \brief Synchronous operation to pass to engine. */
  typedef std::function<void(RunContext)> SyncFn;
  /*! \brief Asynchronous operation to pass to engine. */
  typedef std::function<void(RunContext, CallbackOnComplete)> AsyncFn;
  /*! \brief Variable pointer */
  typedef engine::VarHandle VarHandle;
  /*! \brief Operator pointer */
  typedef engine::OprHandle OprHandle;
  /*!
   * \brief Allocate a new variable, the variable can then
   *        be used to schedule the operation concurrently via dependency
   *        patterns.
   * \return The new variable allocated.
   */
  virtual VarHandle NewVariable() = 0;
  /*!
   * \brief Create a new operator. The returned operator could be saved
   *        externally so that it could be resued for scheduling.
   * \param fn The execution function.
   * \param const_vars The variables that current operation will use but not
   *                   mutate.
   * \param mutable_vars The variables that current operation will mutate.
   * \param prop Property of the function.
   * \return The new operator allocated.
   */
  virtual OprHandle NewOperator(AsyncFn fn,
                                std::vector<VarHandle> const& const_vars,
                                std::vector<VarHandle> const& mutable_vars,
                                FnProperty prop = FnProperty::kNormal) = 0;
  /*!
   * \brief Delete the given operator.
   * \param op The operator to delete.
   *
   * The delete will not happen immediately, but will wait until all the
   * operations using this operator are completed.
   */
  virtual void DeleteOperator(OprHandle op) = 0;
  /*!
   * \brief Push an operator to the engine.
   * \param op The operator to push.
   * \param exec_ctx Execution context.
   */
  virtual void Push(OprHandle op, Context exec_ctx) = 0;
  /*!
   * \brief Push an asynchronous operation to the engine.
   * \param exec_fun Execution function, this function takes a parameter
   *                 on_complete that must be called when the execution
   *                 completes.
   * \param exec_ctx Execution context.
   * \param const_vars The variables that current operation will use but not
   *                   mutate.
   * \param mutable_vars The variables that current operation will mutate.
   * \param prop Property of the function.
   */
  virtual void PushAsync(AsyncFn exec_fun, Context exec_ctx,
                         std::vector<VarHandle> const& const_vars,
                         std::vector<VarHandle> const& mutable_vars,
                         FnProperty prop = FnProperty::kNormal) = 0;
  /*!
   * \brief Schedule the deletion of a variable.
   *
   * The delete will not happen immediately, but will wait until all the
   * operations depending on var are completed.
   *
   * \param delete_fun A function that will be called after the variable is
   *                   deleted.
   * \param exec_ctx Execution context.
   * \param var The variable to be deleted.
   */
  virtual void DeleteVariable(SyncFn delete_fn,
                              Context exec_ctx,
                              VarHandle var) = 0;
  /*!
   * \brief Wait for a variable.
   * \param var The variable we should wait for. This function returns when the
   *            variable is ready.
   */
  virtual void WaitForVar(VarHandle var) = 0;
  /*!
   * \brief Wait until all the activity of engine finishes.
   */
  virtual void WaitForAll() = 0;
  /*!\brief virtual destructor */
  virtual ~Engine() noexcept(false) {}
  /*!
   * \return Engine singleton.
   */
  static Engine* Get();
  /*!
   * \brief Get shared pointer reference to engine singleton.
   *  Most user should not call this function.
   *  This function is called by another singleton X who requires
   *  engine to be destructed after X.
   *
   * \return A shared pointer to Engine singleton.
   */
  static std::shared_ptr<Engine> _GetSharedRef();
  /*!
   * \brief Push an synchronous operation to the engine.
   * \param exec_fn Execution function that executes the operation.
   * \param exec_ctx Execution context.
   * \param const_vars The variables that current operation will use but not
   *                   mutate.
   * \param mutable_vars The variables that current operation will mutate.
   * \param prop Property of the function.
   * \tparam SyncFn the synchronous function to be pushed.
   */
  template<typename SyncFn>
  inline void PushSync(SyncFn exec_fn, Context exec_ctx,
                       std::vector<VarHandle> const& const_vars,
                       std::vector<VarHandle> const& mutable_vars,
                       FnProperty prop = FnProperty::kNormal) {
    this->PushAsync([exec_fn](RunContext ctx, CallbackOnComplete on_complete) {
        exec_fn(ctx);
        on_complete();
      }, exec_ctx, const_vars, mutable_vars, prop);
  }

 protected:
  /*!
   * \brief factory function to create OnComplete callback.
   * \param callback th static callback function.
   * \param param the paramter passed to callback.
   */
  inline CallbackOnComplete CreateCallback(
      void (*callback)(Engine *, void *), void *param) {
    CallbackOnComplete ret;
    ret.callback_ = callback;
    ret.engine_ = this;
    ret.param_ = param;
    return ret;
  }
};  // class Engine
#endif  // DMLC_USE_CXX11
}  // namespace mxnet
#endif  // MXNET_ENGINE_H_
