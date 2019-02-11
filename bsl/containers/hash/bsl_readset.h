/***************************************************************************
 * 
 * Copyright (c) 2008 Baidu.com, Inc. All Rights Reserved
 * $Id: bsl_readset.h,v 1.9 2009/04/07 06:35:53 xiaowei Exp $ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file bsl_readset.h
 * @author yingxiang(yingxiang@baidu.com)
 * @date 2008/08/14 13:39:16
 * @version $Revision: 1.9 $   2010/10/15 modified by zhjw(zhujianwei@baidu.com)
 * @brief A read-only hash table
 *  
 **/


#ifndef  __BSL_READSET_H_
#define  __BSL_READSET_H_

#include "bsl/alloc/bsl_alloc.h"
#include "bsl/utils/bsl_utils.h"
#include "bsl/containers/hash/bsl_hashutils.h"
#include "bsl/archive/bsl_serialization.h"
#include "bsl/exception/bsl_exception.h"
#include <string.h>
#include <algorithm>

namespace bsl{

template <class _Key,
		 class _HashFunction = xhash<_Key>,
		 class _EqualFunction = std::equal_to<_Key>,
		 class _Alloc = bsl_alloc<_Key> >
class readset{
private:
	//data structure types
	typedef _Key key_type;
	typedef size_t size_type;
	typedef size_t sign_type;//Hash signature type, generated by HashFunction
	typedef readset<_Key, _HashFunction, _EqualFunction, _Alloc> self_type;

	struct _node_type{
		size_type idx, pos, hash_value;
	}; 
public:
	enum{ DEFAULT_BUCKET_SIZE = 65535 };
	typedef key_type* iterator;
	typedef const key_type* const_iterator;
	typedef _node_type node_type;
private:
	//Allocator types
	typedef typename _Alloc::template rebind <iterator> :: other itr_alloc_type;
	typedef typename _Alloc::template rebind <key_type> :: other key_alloc_type;
	typedef typename _Alloc::template rebind <node_type> :: other node_alloc_type;
public:
	//compare functions 
	static int _cmp_hash(node_type __a, node_type __b){ 
		return __a.hash_value < __b.hash_value; 
	}
	static int _cmp_idx(node_type __a, node_type __b){ 
		return __a.idx < __b.idx; 
	}
private:
	//data
	iterator* _bucket;
	iterator _storage;
	size_type _hash_size;		/**<哈希桶的大小        */
	size_type _size;		  /**<哈希中元素的个数        */
	size_type _mem_size;	/**< 内存空间，=_hash_size+1    */
	//Object Function
	_HashFunction _hashfun;
	_EqualFunction _eqfun;
	//Allocators
	itr_alloc_type _itr_alloc;
	key_alloc_type _key_alloc;
	node_alloc_type _node_alloc;
public:

	/* *
	 * @brief 默认构造函数
	 * 无异常
	 * 需调create
	 * */
	readset(){
		_reset();
	}

	/**
	 * @brief 带桶大小的构造函数
	 *        如果构造失败，将销毁对象
	 * 抛异常
	 * 不需调create         
	 * @param  [in] __bucket_size   : const size_type
	 * @param  [in] __hash_func     : const _HashFunction&
	 * @param  [in] __equal_func    : const _EqualFunction&
	 * @return
	 * @retval
	 * @see
	 * @author liaoshangbin
	 * @data 2010/08/30 15:16:58
	 **/
	readset(const size_type __bucket_size, 
			const _HashFunction& __hash_func = _HashFunction(), 
			const _EqualFunction& __equal_func = _EqualFunction()){
		_reset();
		if (create(__bucket_size,__hash_func,__equal_func) != 0) {
			destroy();
			throw BadAllocException()<<BSL_EARG
				<<"create error when create readset bitems "<<__bucket_size;
		}
	}

	/**
	 * @brief 拷贝构造函数，抛异常
	 *        如果构造失败，将销毁对象
	 *          
	 * @param  [in] other   : const readset&
	 * @return
	 * @retval
	 * @see
	 * @author liaoshangbin
	 * @data 2010/08/30 15:19:10
	 **/
	readset(const self_type& other) {
		_reset();
		if (assign(other.begin(),other.end()) != 0) {
			destroy();
			throw BadAllocException()<<BSL_EARG<<"assign readset error";
		}
	}

	/**
	 * @brief 赋值运算符
	 *        先获得自身的一个拷贝，如果赋值失败，将恢复成原来的readset
	 *          
	 * @param  [in] other	: const readset&
	 * @return
	 * @retval
	 * @see
	 * @author liaoshangbin
	 * @data 2010/08/30 15:20:16
	 **/
	self_type& operator = (const self_type& other) {
		if (this != &other) {
			readset self_(*this);
			if (assign(other.begin(),other.end()) != 0) {
				destroy();
				*this = self_;
				throw BadAllocException()<<BSL_EARG<<"assign readset error";
			}
		}
		return *this;
	}

	/**
	 * @brief 创建hash表，指定桶大小和hash函数、比较函数
	 *
	 * @param [in] __bucket_size   : const size_type 
	 *				桶大小
	 * @param [in] __hash_func   : const _HashFunction& 
	 *				hash函数，参数为__key&,返回值为size_t
	 * @param [in] __equal_func   : const _EqualFunction&
	 *				比较函数，默认使用==
	 * @return  int 
	 *			<li> -1：出错
	 *			<li> 0：成功
	 * @retval   
	 * @see 
	 * @note 如果hash桶已经创建，第二次调用create时会先destroy之前的hash表
	 * @author yingxiang
	 * @date 2008/08/14 11:24:28
	**/
	int create(const size_type __bucket_size = size_type(DEFAULT_BUCKET_SIZE), 
			const _HashFunction& __hash_func = _HashFunction(), 
			const _EqualFunction& __equal_func = _EqualFunction()){
		if(_hash_size){
			destroy();//防止重复create
		}

		_itr_alloc.create();
		_key_alloc.create();
		_node_alloc.create();

		_hashfun = __hash_func;
		_eqfun = __equal_func;
		_hash_size = __bucket_size;

		if(_hash_size <= 0){
			return -1;
		}

		_mem_size = _hash_size + 1;//可以优化查询速度
		_bucket = _itr_alloc.allocate(_mem_size);
		if(0 == _bucket){
			return -1;
		}	
		return 0;
	}

    /* *
     * @brief 判断readset是否已create
     * @author zhujianwei
     * @date 2010/12/13
     * */
    bool is_created() const{
        return (_bucket != 0);
    }

	/**
	 * @brief 向hash表中赋初始元素。将[start, finish)区间的元素插入hash表<br>
	 *			[start, finish)为左闭右开区间 <br>
	 *			如果assign之前未显式调用create，assign会自动调用create <br>
	 *
	 * @param [in] __start   : _Iterator 起始位置
	 * @param [in] __finish   : _Iterator 终止位置
	 * @return  int 
	 *			<li> 0 : 成功
	 *			<li> -1 : 其他错误
	 * @retval   
	 * @see 
	 * @note 插入的key值不可以有相同
	 * @author yingxiang
	 * @date 2008/08/14 11:32:20
	**/
	template <class _Iterator>
	int assign(const _Iterator __start, const _Iterator __finish){
		if(_size) {
			clear();
		}

		_size = std::distance(__start, __finish);
		
		if (_size == 0) {
			return 0;
		}
		//如果未显示调用create，则自动创建，桶大小为4倍
		if(0 == _hash_size) {
			if(create(_size * 4)){
				return -1;
			}
		}

		iterator p = _key_alloc.allocate(_size);
		if(0 == p) {
			_size = 0;
			return -1;
		}
		_storage = p;

		node_type* temp = _node_alloc.allocate(_size);
		if(0 == temp) {
			_key_alloc.deallocate(p, _size);
			_size = 0;
			return -1;
		}

		_Iterator itr = __start;
		size_type i, j;
		for(i = 0; itr != __finish; ++itr, ++i){
			temp[i].idx = i;
			temp[i].hash_value = _hashfun(*itr) % _hash_size;
		}

		//为每个萝卜分配一个坑
		std::sort(temp, temp + _size, _cmp_hash);
		for(i = 0; i < _size; ++i){
			temp[i].pos = i;
		}

		//计算桶入口
		for(i = 0, j = 0; j < _mem_size; ++j){
			while(i < _size && temp[i].hash_value < j){
				++i;
			}
			_bucket[j] = _storage + i;
		}

		//拷贝内存到存储区
		std::sort(temp, temp + _size, _cmp_idx);
		for(i = 0, itr = __start; i < _size; ++i, ++itr){
			bsl::bsl_construct(&_storage[temp[i].pos], *itr);
		}

		_node_alloc.deallocate(temp, _size);

		return 0;
	}

	/**
	 * @brief 根据Key值查询
	 *
	 * @param [in] __key   : const key_type& 要查询的Key值
	 * @return  int 
	 *			<li> HASH_EXIST : 存在
	 *			<li> HASH_NOEXIST : 不存在
	 * @retval   
	 * @see 
	 * @author yingxiang
	 * @date 2008/08/14 15:50:34
	**/
	int get(const key_type& __key) const{
		if(0 == _size){
			return HASH_NOEXIST;
		}
		sign_type hash_value = _hashfun(__key) % _hash_size;
		for(iterator p = _bucket[hash_value]; p < _bucket[hash_value + 1]; ++p){
			if(_eqfun(__key, *p)){
				return HASH_EXIST;
			}
		}
		return HASH_NOEXIST;
	}

	/**
	 * @brief 返回hash表中的元素个数
	 *
	 * @return  size_type 元素个数
	 * @retval   
	 * @see 
	 * @author yingxiang
	 * @date 2008/08/14 15:53:51
	**/
	size_type size() const {
		return _size;
	}

	/**
	 * @brief 返回hash表桶大小
	 *
	 * @return  size_type 桶大小
	 * @retval   
	 * @see 
	 * @author yingxiang
	 * @date 2008/08/14 15:54:30
	**/
	size_type bucket_size() const {
		return _hash_size;
	}

	/**
	 * @brief 销毁hash表
	 *
	 * @return  void 
	 * @retval   
	 * @see 
	 * @author yingxiang
	 * @date 2008/08/14 15:54:52
	**/
	void destroy(){
		clear();
		if(_mem_size > 0 && NULL != _bucket){
			_itr_alloc.deallocate(_bucket, _mem_size);
			_mem_size = _hash_size = 0;
			_bucket = NULL;
		}
		_key_alloc.destroy();
		_itr_alloc.destroy();
		_node_alloc.destroy();
	}

	/**
	 * @brief 清空hash表中的元素
	 *
	 * @return  void 
	 * @retval   
	 * @see 
	 * @author yingxiang
	 * @date 2008/08/14 15:55:07
	**/
	void clear(){
		if(_size > 0 && NULL != _storage){
			bsl::bsl_destruct(_storage, _storage + _size);
			_key_alloc.deallocate(_storage, _size);
			_size = 0;
			_storage = NULL;
		}
	}

	/**
	 * @brief 存储区的头部指针。用于遍历hash表中的元素。仅用于访问操作
	 *
	 * @return  const iterator 头部指针。空容器返回NULL
	 * @retval   
	 * @see 
	 * @author yingxiang
	 * @date 2008/08/21 15:01:53
	**/
	iterator begin(){
		return _storage;
	}

	/**
	 * @brief 存储区的头部指针
	 */
	const_iterator begin() const {
		return _storage;
	}

	/**
	 * @brief 存储区的尾部指针
	 *
	 * @return  const iterator 尾部指针（指向的内存单元不可以被访问� 
	 * @retval   
	 * @see 
	 * @author yingxiang
	 * @date 2008/08/21 15:02:31
	**/
	iterator end(){
		return _storage + size();
	}
	
	/**
	 * @brief 存储区的尾部指针
	 */
	const_iterator end() const {
		return _storage + size();
	}

	~readset(){
		destroy();
	}

	/**
	 * @brief 串行化。将hash表串行化到指定的流
	 *
	 * @param [in/out] ar   : _Archive& 串行化流
	 * @return  int 
	 *			<li> 0 : 成功
	 *			<li> -1 : 失败
	 * @retval   
	 * @see 
	 * @author yingxiang
	 * @date 2008/08/14 16:08:11
	**/
	template <class _Archive>
	int serialization(_Archive & ar) {
		if(bsl::serialization(ar, _hash_size) != 0){
			goto _ERR_S;
		}
		if(bsl::serialization(ar, _size) != 0) {
			goto _ERR_S;
		}
		size_type i;
		for(i = 0; i < _size; ++i){
			if(bsl::serialization(ar, _storage[i]) != 0){
				goto _ERR_S;
			}
		}
		return 0;
_ERR_S:
		return -1;
	}

	/**
	 * @brief 反串行化。从串行化流中获取数据重建hash表
	 *
	 * @param [in] ar   : _Archive& 串行化流
	 * @return  int 
     *          <li> 0 : 成功
     *          <li> -1 : 失败
	 * @retval   
	 * @see 
	 * @author yingxiang
	 * @date 2008/08/14 16:08:19
	**/
	template <class _Archive>
	int deserialization(_Archive & ar){
		size_type fsize, hashsize, i;
		iterator tmp_storage = NULL;

		destroy();

		if(bsl::deserialization(ar, hashsize) != 0){
			goto _ERR_DS;
		}
		if(bsl::deserialization(ar, fsize) != 0){
			goto _ERR_DS;
		}
		if(hashsize <= 0 || fsize <= 0) {
			goto _ERR_DS;
		}

		if(create(hashsize) != 0){
			goto _ERR_DS;
		}

		tmp_storage = _key_alloc.allocate(fsize);
		if(NULL == tmp_storage){ 
			goto _ERR_DS;
		}

		for(i = 0; i < fsize; ++i){
			bsl::bsl_construct(&tmp_storage[i]);
		}

		for(i = 0; i < fsize; ++i){
			if(bsl::deserialization(ar, tmp_storage[i]) != 0){
				goto _ERR_DS;
			}
		}

		if(assign(tmp_storage, tmp_storage + fsize) != 0){
			goto _ERR_DS;
		}

		bsl::bsl_destruct(tmp_storage, tmp_storage + fsize);
		_key_alloc.deallocate(tmp_storage, fsize);
		return 0;
_ERR_DS:
		if(NULL != tmp_storage){
			bsl::bsl_destruct(tmp_storage, tmp_storage + fsize);
			_key_alloc.deallocate(tmp_storage, fsize);
		}
		destroy();
		return -1;
	}
private:
	void _reset(){
		_bucket = NULL;
		_storage = NULL;
		_hash_size = 0;
		_mem_size = 0;
		_size = 0;
	}

};//class readset

}//namespace bsl






#endif  //__BSL_READSET_H_

/* vim: set ts=4 sw=4 sts=4 tw=100 noet: */
