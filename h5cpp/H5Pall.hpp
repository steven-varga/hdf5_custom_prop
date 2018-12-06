/*
 * Copyright (c) 2018 vargaconsulting, Toronto,ON Canada
 * Author: Varga, Steven <steven@vargaconsulting.ca>
 *
 */

#ifndef  H5CPP_PALL_HPP
#define  H5CPP_PALL_HPP

namespace h5 { namespace impl {
	/* proxy object that gets converted to property_id with restriction that 
	 * only same class properties may be daisy chained */
	template <class Derived, class phid_t>
	struct prop_base {
		prop_base(){ }
		// removed ctor
		~prop_base(){
			if( H5Iis_valid( handle ) ){
				H5Pclose( handle );
			}
	  	}

		template <class R> 	const prop_base<Derived, phid_t>&
		operator|( const R& rhs ) const {
			rhs.copy( handle );
			return *this;
		 }

		void copy(::hid_t handle_) const { /*CRTP idiom*/
			static_cast<const Derived*>(this)->copy_impl( handle_ );
		}
		/*transfering ownership to managed handle*/
		operator phid_t( ) const {
			static_cast<const Derived*>(this)->copy_impl( handle );
			H5Iinc_ref( handle ); /*keep this alive */
			return phid_t{handle};
		}
		::hid_t handle;
		using type = phid_t;
	};
	// definition
	template <class phid_t, defprop_t init, class capi, typename capi::fn_t capi_call> struct prop_t;
	/* CAPI macros are sequence calls: (H5OPEN, register_property_class), these are all meant to be read only/copied from,
	 */
	#define H5CPP__capicall( name, default_id ) ::hid_t inline default_##name(){ return default_id; } 	\
			template <class capi, typename capi::fn_t capi_call>                                        \
				using name##_call = prop_t<h5::name##_t, default_##name,  capi, capi_call>; 	        \
			template <class... args> using name##_args = impl::capi_t<args...>; 						\

	H5CPP__capicall( acpl, H5P_ATTRIBUTE_CREATE) H5CPP__capicall( dapl, H5P_DATASET_ACCESS )
	H5CPP__capicall( dxpl, H5P_DATASET_XFER )    H5CPP__capicall( dcpl, H5P_DATASET_CREATE )
	H5CPP__capicall( tapl, H5P_DATATYPE_ACCESS ) H5CPP__capicall( tcpl, H5P_DATATYPE_CREATE )
	H5CPP__capicall( fapl, H5P_FILE_ACCESS )     H5CPP__capicall( fcpl, H5P_FILE_CREATE )
	H5CPP__capicall( fmpl, H5P_FILE_MOUNT )

	H5CPP__capicall( gapl, H5P_GROUP_ACCESS)     H5CPP__capicall( gcpl, H5P_GROUP_CREATE )
	H5CPP__capicall( lapl, H5P_LINK_ACCESS)      H5CPP__capicall( lcpl, H5P_LINK_CREATE	 )
	H5CPP__capicall( ocpl, H5P_OBJECT_COPY)      H5CPP__capicall( ocrl, H5P_OBJECT_CREATE )
	H5CPP__capicall( scpl, H5P_STRING_CREATE)
	#undef H5CPP__defid

	/*property with a capi function call with some arguments, also is the base for other properties */
	template <class phid_t, defprop_t init, class capi, typename capi::fn_t capi_call>
	struct prop_t : prop_base<prop_t<phid_t,init,capi,capi_call>,phid_t> {
		prop_t( typename capi::args_t values ) : args( values ) {
			H5CPP_CHECK_NZ( (this->handle = H5Pcreate(init())),
				   h5::error::property_list::misc, "failed to create property");
		}
		prop_t(){
			H5CPP_CHECK_NZ( (this->handle = H5Pcreate(init())),
				   h5::error::property_list::misc, "failed to create property");
		}
		void copy_impl(::hid_t id) const {
			//int i = capi_call + 1;
			/*CAPI needs `this` hid_t id passed along */
			capi_t capi_args = std::tuple_cat( std::tie(id), args );
			H5CPP_CHECK_NZ( compat::apply(capi_call, capi_args),
					h5::error::property_list::argument,"failed to parse arguments...");
		}
		using base_t =  prop_base<prop_t<phid_t,init,capi,capi_call>,phid_t>;
		using args_t = typename capi::args_t;
		using capi_t = typename capi::type;
		using type = phid_t;
		args_t args;
	};

	/* T prop is much similar to other properties, except the value needs HDF5 type info and passed by
	 * const void* pointer type. This is reflected by templating `prop_t` 
	 */
	template <class phid_t, defprop_t init, class capi, typename capi::fn_t capi_call, class T>
	struct tprop_t final : prop_t<phid_t,init,capi,capi_call> { // ::hid_t,const void*
		tprop_t( T value ) : base_t( std::make_tuple( h5::copy<T>( h5::dt_t<T>() ), &this->value) ), value(value) {
		}
		~tprop_t(){ // don't forget that the first argument is a HDF5 type info, that needs closing
			if( H5Iis_valid( std::get<0>(this->args) ) )
				H5Tclose( std::get<0>(this->args) );
		}
		using type = phid_t;
		using base_t = prop_t<phid_t,init,capi,capi_call>;
		T value;
	};
	/* takes an arbitrary length of hsize_t sequence and calls H5Pset_xxx
	 */
	template <class phid_t, defprop_t init, class capi, typename capi::fn_t capi_call>
	struct aprop_t final : prop_t<phid_t,init,capi,capi_call> {
		aprop_t( std::initializer_list<hsize_t> values )
			: base_t( std::make_tuple( values.size(), this->values) ) {
			std::copy( std::begin(values), std::end(values), this->values);
		}
		hsize_t values[H5CPP_MAX_RANK];
		using type = phid_t;
		using base_t =  prop_t<phid_t,init,capi,capi_call>;
	};

	// only data control property list set_chunk has this pattern, lets allow to define CAPI argument lists 
	// the same way as with others
	template <class capi, typename capi::fn_t capi_call>
		using dcpl_acall = aprop_t<h5::dcpl_t,default_dcpl, capi,capi_call>;
	// only data control property list set_value has this pattern, lets allow to define CAPI argument lists 
	// the same way as with others
	template <class capi, typename capi::fn_t capi_call, class T> using dcpl_tcall = tprop_t<h5::dcpl_t,default_dcpl, capi, capi_call, T>;
}}

/** impl::property_group<H5Pset_property, args...> args... ::= all argument types except the first `this` hid_t prop_ID
 *  since it is implicitly passed by template class. 
 */ 
namespace h5 {

// FILE CREATION PROPERTY LISTS
using userblock                = impl::fcpl_call< impl::fcpl_args<hid_t,hsize_t>,           H5Pset_userblock>;
using istore_k                 = impl::fcpl_call< impl::fcpl_args<hid_t,unsigned>,          H5Pset_istore_k>;
using file_space_page_size     = impl::fcpl_call< impl::fcpl_args<hid_t,hsize_t>,           H5Pset_file_space_page_size>;
using shared_mesg_nindexes     = impl::fcpl_call< impl::fcpl_args<hid_t,unsigned>,          H5Pset_shared_mesg_nindexes>;
using sizes                    = impl::fcpl_call< impl::fcpl_args<hid_t,size_t,size_t>,     H5Pset_sizes>;
using sym_k                    = impl::fcpl_call< impl::fcpl_args<hid_t,unsigned,unsigned>, H5Pset_sym_k>;
using shared_mesg_index        = impl::fcpl_call< impl::fcpl_args<hid_t,unsigned,unsigned,unsigned>,H5Pset_shared_mesg_index>;
using shared_mesg_phase_change = impl::fcpl_call< impl::fcpl_args<hid_t,unsigned,unsigned>, H5Pset_shared_mesg_phase_change>;
// FILE ACCESS PROPERTY LISTS
using fclose_degree            = impl::fapl_call< impl::fapl_args<hid_t,H5F_close_degree_t>, H5Pset_fclose_degree>;
using fapl_core                = impl::fapl_call< impl::fapl_args<hid_t,size_t,hbool_t>, H5Pset_fapl_core>;
using core_write_tracking      = impl::fapl_call< impl::fapl_args<hid_t,hbool_t,size_t>, H5Pset_core_write_tracking>;
using fapl_family              = impl::fapl_call< impl::fapl_args<hid_t,hsize_t,hid_t>, H5Pset_fapl_family>;
using family_offset            = impl::fapl_call< impl::fapl_args<hid_t,hsize_t>, H5Pset_family_offset>;
using fapl_log                 = impl::fapl_call< impl::fapl_args<hid_t,const char*,unsigned long long,size_t>,H5Pset_fapl_log>;
using multi_type               = impl::fapl_call< impl::fapl_args<hid_t,H5FD_mem_t>,H5Pset_multi_type>;
using fapl_split               = impl::fapl_call< impl::fapl_args<hid_t,const char*, hid_t,const char*,hid_t>,H5Pset_fapl_split>;
using file_image               = impl::fapl_call< impl::fapl_args<hid_t,void*,size_t>,H5Pset_file_image>;
using meta_block_size          = impl::fapl_call< impl::fapl_args<hid_t,hsize_t>,H5Pset_meta_block_size>;
using page_buffer_size         = impl::fapl_call< impl::fapl_args<hid_t,size_t,unsigned,unsigned>,H5Pset_page_buffer_size>;
using sieve_buf_size           = impl::fapl_call< impl::fapl_args<hid_t,size_t>,H5Pset_sieve_buf_size>;
using alignment                = impl::fapl_call< impl::fapl_args<hid_t,hsize_t, hsize_t>,H5Pset_alignment>;
using cache                    = impl::fapl_call< impl::fapl_args<hid_t,int,size_t,size_t,double>,H5Pset_cache>;
using elink_file_cache_size    = impl::fapl_call< impl::fapl_args<hid_t,unsigned>,H5Pset_elink_file_cache_size>;
using evict_on_close           = impl::fapl_call< impl::fapl_args<hid_t,hbool_t>,H5Pset_evict_on_close>;
using metadata_read_attempts   = impl::fapl_call< impl::fapl_args<hid_t,unsigned>,H5Pset_metadata_read_attempts>;
using mdc_config               = impl::fapl_call< impl::fapl_args<hid_t,H5AC_cache_config_t *>,H5Pset_mdc_config>;
using mdc_image_config         = impl::fapl_call< impl::fapl_args<hid_t,H5AC_cache_image_config_t*>,H5Pset_mdc_image_config>;
using mdc_log_options          = impl::fapl_call< impl::fapl_args<hid_t,hbool_t,const char*,hbool_t>,H5Pset_mdc_log_options>;
using libver_bounds            = impl::fapl_call< impl::fapl_args<hid_t,H5F_libver_t,H5F_libver_t>,H5Pset_libver_bounds>;
namespace flag {
	using fapl_sec2            = impl::fapl_call< impl::fapl_args<hid_t>,H5Pset_fapl_sec2>;
	using fapl_stdio           = impl::fapl_call< impl::fapl_args<hid_t>,H5Pset_fapl_stdio>;
}
const static flag::fapl_sec2  sec2;
const static flag::fapl_stdio stdio;
const static h5::fclose_degree fclose_degree_weak{H5F_CLOSE_WEAK};
const static h5::fclose_degree fclose_degree_semi{H5F_CLOSE_SEMI};
const static h5::fclose_degree fclose_degree_strong{H5F_CLOSE_STRONG};
const static h5::fclose_degree fclose_degree_default{H5F_CLOSE_DEFAULT	};
// GROUP CREATION PROPERTY LISTS
using local_heap_size_hint     = impl::gcpl_call< impl::gcpl_args<hid_t,size_t>,H5Pset_local_heap_size_hint>;
using link_creation_order      = impl::gcpl_call< impl::gcpl_args<hid_t,unsigned>,H5Pset_link_creation_order>;
using est_link_info            = impl::gcpl_call< impl::gcpl_args<hid_t,unsigned, unsigned>,H5Pset_est_link_info>;
using link_phase_change        = impl::gcpl_call< impl::gcpl_args<hid_t,unsigned, unsigned>,H5Pset_link_phase_change>;
// GROUP ACCESS PROPERTY LISTS
// LINK CREATION PROPERTY LISTS
using char_encoding            = impl::lcpl_call< impl::lcpl_args<hid_t,H5T_cset_t>,H5Pset_char_encoding>;
using create_intermediate_group= impl::lcpl_call< impl::lcpl_args<hid_t,unsigned>,H5Pset_create_intermediate_group>;
const static h5::char_encoding ascii{H5T_CSET_ASCII};
const static h5::char_encoding utf8{H5T_CSET_UTF8};
const static h5::create_intermediate_group create_path{1};
// LINK ACCESS PROPERTY LISTS
using nlinks                   = impl::lapl_call< impl::lapl_args<hid_t,size_t>,H5Pset_nlinks>;
using elink_cb                 = impl::lapl_call< impl::lapl_args<hid_t,H5L_elink_traverse_t, void*>,H5Pset_elink_cb>;
using elink_prefix             = impl::lapl_call< impl::lapl_args<hid_t,const char*>,H5Pset_elink_prefix>;
using elink_fapl               = impl::lapl_call< impl::lapl_args<hid_t,hid_t>,H5Pset_elink_fapl>;
using elink_acc_flags          = impl::lapl_call< impl::lapl_args<hid_t,unsigned>,H5Pset_elink_acc_flags>;
// DATA CREATION PROPERTY LISTS
using layout                   = impl::dcpl_call< impl::dcpl_args<hid_t,H5D_layout_t>,H5Pset_layout>;
using deflate                  = impl::dcpl_call< impl::dcpl_args<hid_t,unsigned>,H5Pset_deflate>;
using gzip                     = deflate;
using fill_time                = impl::dcpl_call< impl::dcpl_args<hid_t,H5D_fill_time_t>,H5Pset_fill_time>;
using alloc_time               = impl::dcpl_call< impl::dcpl_args<hid_t,H5D_alloc_time_t>,H5Pset_alloc_time>;
using chunk_opts               = impl::dcpl_call< impl::dcpl_args<hid_t,unsigned>,H5Pset_chunk_opts>;
template<class T> /*tcall ::= templated call with T*/
using fill_value               = impl::dcpl_tcall< impl::dcpl_args<hid_t,hid_t,const void*>, H5Pset_fill_value, T>;
using chunk                    = impl::dcpl_acall< impl::dcpl_args<hid_t,int,const hsize_t*>, H5Pset_chunk>; /*acall ::= array call of hsize_t */
namespace flag{
	using fletcher32           = impl::dcpl_call< impl::dcpl_args<hid_t>,H5Pset_fletcher32>;
	using shuffle              = impl::dcpl_call< impl::dcpl_args<hid_t>,H5Pset_shuffle>;
	using nbit                 = impl::dcpl_call< impl::dcpl_args<hid_t>,H5Pset_nbit>;
}
const static flag::fletcher32 fletcher32;
const static flag::shuffle shuffle;
const static flag::nbit nbit;

const static h5::layout layout_compact{H5D_COMPACT};
const static h5::layout layout_contigous{H5D_CONTIGUOUS};
const static h5::layout layout_chunked{H5D_CHUNKED};
const static h5::layout layout_virtual{H5D_VIRTUAL};
const static h5::fill_time fill_time_ifset{H5D_FILL_TIME_IFSET};
const static h5::fill_time fill_time_alloc{H5D_FILL_TIME_ALLOC};
const static h5::fill_time fill_time_never{H5D_FILL_TIME_NEVER};
const static h5::alloc_time alloc_time_default{H5D_ALLOC_TIME_DEFAULT};
const static h5::alloc_time alloc_time_early{H5D_ALLOC_TIME_EARLY};
const static h5::alloc_time alloc_time_incr{H5D_ALLOC_TIME_INCR};
const static h5::alloc_time alloc_time_late{H5D_ALLOC_TIME_LATE};
const static h5::chunk_opts dont_filter_partial_chunks{H5D_CHUNK_DONT_FILTER_PARTIAL_CHUNKS};
// DATA ACCESS PROPERTY LISTS
using chunk_cache              = impl::fapl_call< impl::fapl_args<hid_t,size_t, size_t, double>,H5Pset_chunk_cache>;
using efile_prefix             = impl::fapl_call< impl::fapl_args<hid_t,const char*>,H5Pset_efile_prefix>;
using virtual_view             = impl::fapl_call< impl::fapl_args<hid_t,H5D_vds_view_t>,H5Pset_virtual_view>;
using virtual_printf_gap       = impl::fapl_call< impl::fapl_args<hid_t,hsize_t>,H5Pset_virtual_printf_gap>;
// DATA TRANSFER PROPERTY LISTS
using buffer                   = impl::dxpl_call< impl::dxpl_args<hid_t,size_t,void*,void*>,H5Pset_buffer>;
using edc_check                = impl::dxpl_call< impl::dxpl_args<hid_t,H5Z_EDC_t>,H5Pset_edc_check>;
using filter_callback          = impl::dxpl_call< impl::dxpl_args<hid_t,H5Z_filter_func_t,void*>,H5Pset_filter_callback>;
using data_transform           = impl::dxpl_call< impl::dxpl_args<hid_t,const char *>,H5Pset_data_transform>;
using type_conv_cb             = impl::dxpl_call< impl::dxpl_args<hid_t,H5T_conv_except_func_t,void*>,H5Pset_type_conv_cb>;
using hyper_vector_size        = impl::dxpl_call< impl::dxpl_args<hid_t,size_t>,H5Pset_hyper_vector_size>;
using btree_ratios             = impl::dxpl_call< impl::dxpl_args<hid_t,double,double,double>,H5Pset_btree_ratios>;
// OBJECT CREATION PROPERTY LISTS
using ocreate_intermediate_group = impl::ocrl_call< impl::ocrl_args<hid_t,unsigned>,H5Pset_create_intermediate_group>;
using obj_track_times            = impl::ocrl_call< impl::ocrl_args<hid_t,hbool_t>,H5Pset_obj_track_times>;
using attr_phase_change          = impl::ocrl_call< impl::ocrl_args<hid_t,unsigned,unsigned>,H5Pset_attr_phase_change>;
using attr_creation_order        = impl::ocrl_call< impl::ocrl_args<hid_t,unsigned>,H5Pset_attr_creation_order>;
//static h5::ocreate_intermediate_group            ocreate_intermediate_group{1};
// OBJECT COPY PROPERTY LISTS
using copy_object                = impl::ocpl_call< impl::ocpl_args<hid_t,unsigned>,H5Pset_copy_object>;
using mcdt_search_cb             = impl::ocpl_call< impl::ocpl_args<hid_t,H5O_mcdt_search_cb_t,void*>,H5Pset_mcdt_search_cb>;
const static h5::copy_object shallow_hierarchy{H5O_COPY_SHALLOW_HIERARCHY_FLAG};
const static h5::copy_object expand_soft_link{H5O_COPY_EXPAND_SOFT_LINK_FLAG};
const static h5::copy_object expand_ext_link{H5O_COPY_EXPAND_EXT_LINK_FLAG};
const static h5::copy_object expand_reference{H5O_COPY_EXPAND_REFERENCE_FLAG};
const static h5::copy_object copy_without_attr{H5O_COPY_WITHOUT_ATTR_FLAG};
const static h5::copy_object merge_commited_dtype{H5O_COPY_MERGE_COMMITTED_DTYPE_FLAG};


#ifdef H5_HAVE_PARALLEL
using fapl_mpio                  = impl::fapl_call< impl::fapl_args<hid_t,MPI_Comm, MPI_Info>,H5Pset_fapl_mpio>;
using all_coll_metadata_ops      = impl::fapl_call< impl::fapl_args<hid_t,hbool_t>,H5Pset_all_coll_metadata_ops>;
using coll_metadata_write        = impl::fapl_call< impl::fapl_args<hid_t,hbool_t>,H5Pset_coll_metadata_write>;
using gc_reference               = impl::fapl_call< impl::fapl_args<hid_t,unsigned>,H5Pset_gc_reference>;
using small_data_block_size      = impl::fapl_call< impl::fapl_args<hid_t,hsize_t>,H5Pset_small_data_block_size>;
using object_flush_cb            = impl::fapl_call< impl::fapl_args<hid_t,H5F_flush_cb_t,void*>,H5Pset_object_flush_cb>;
using all_coll_metadata_ops      = impl::fapl_call< impl::fapl_args<hid_t,hbool_t>,H5Pset_all_coll_metadata_ops>;
using all_coll_metadata_ops,     = impl::lapl_call< impl::lapl_args<hid_t,hbool_t>,H5Pset_all_coll_metadata_ops>;
using all_coll_metadata_ops      = impl::gapl_call< impl::gapl_args<hid_t,hbool_t>,H5Pset_all_coll_metadata_ops>;
using dxpl_mpio                  = impl::dxpl_call< impl::dxpl_args<hid_t,H5FD_mpio_xfer_t>,H5Pset_dxpl_mpio>;
using dxpl_mpio_chunk_opt        = impl::dxpl_call< impl::dxpl_args<hid_t,H5FD_mpio_chunk_opt_t>,H5Pset_dxpl_mpio_chunk_opt>;
using dxpl_mpio_chunk_opt_num    = impl::dxpl_call< impl::dxpl_args<hid_t,unsigned>,H5Pset_dxpl_mpio_chunk_opt_num>;
using mpio_chunk_opt_ratio       = impl::dxpl_call< impl::dxpl_args<hid_t,unsigned>,H5Pset_mpio_chunk_opt_ratio>;
using mpio_collective_opt        = impl::dxpl_call< impl::dxpl_args<hid_t,H5FD_mpio_collective_opt_t>,H5Pset_mpio_collective_opt>;
using dxpl_mpio                  = impl::dxpl_call< impl::dxpl_args<hid_t,H5FD_mpio_xfer_t*>,H5Pset_dxpl_mpio>;
using mpio_actual_chunk_opt_mode = impl::dxpl_call< impl::dxpl_args<hid_t, H5D_mpio_actual_chunk_opt_mode_t *>,H5Pset_mpio_actual_chunk_opt_mode>;
using mpio_actual_io_mode        = impl::dxpl_call< impl::dxpl_args<hid_t,H5D_mpio_actual_io_mode_t* >,H5Pset_mpio_actual_io_mode>;
using mpio_no_collective_cause   = impl::dxpl_call< impl::dxpl_args<hid_t,uint32_t *,uint32_t *>,H5Pset_mpio_no_collective_cause>;
#endif
}

namespace h5 { namespace notimplemented_yet { // OBJECT COPY PROPERTY LISTS
	//using char_encoding_ =              = impl::fapl_call< impl::fapl_args<hid_t, >,H5Pset_char_encoding, H5T_cset_t)
	//static h5::char_encoding_ ascii{H5T_CSET_ASCII};
	//static h5::char_encoding_ utf8{H5T_CSET_UTF8};
    //FIXME: using fapl_direct =  impl::fapl_call< impl::fapl_args<hid_t, >, .. , size_t, size_t, size_t>;
	//TODO:	using vlen_mem_manager,) too complex to implement
    //TODO:	using append_flush -- too complex to implement
	//NOT MAPPED: fapl_direct, mpiposix, fapl_multi
	//MISSING:	H5CPP__PROPERTYLIST_FLAG(fapl, fapl_windows)
	//MISSING:	using file_image_callbacks, H5_file_image_callbacks_t* >;
}}

namespace h5 {
	const static h5::acpl_t acpl = static_cast<h5::acpl_t>( H5P_DEFAULT );
	const static h5::dapl_t dapl = static_cast<h5::dapl_t>( H5P_DEFAULT );
	const static h5::dcpl_t dcpl = static_cast<h5::dcpl_t>( H5P_DEFAULT);
	const static h5::dxpl_t dxpl = static_cast<h5::dxpl_t>( H5P_DEFAULT );
	const static h5::lcpl_t lcpl = h5::char_encoding{H5T_CSET_UTF8} | h5::create_intermediate_group{1};
	const static h5::fapl_t fapl = static_cast<h5::fapl_t>( H5P_DEFAULT );
	const static h5::fcpl_t fcpl = static_cast<h5::fcpl_t>( H5P_DEFAULT );

	const static h5::acpl_t default_acpl = static_cast<h5::acpl_t>( H5P_DEFAULT );
	const static h5::dapl_t default_dapl = static_cast<h5::dapl_t>( H5P_DEFAULT );
	const static h5::dcpl_t default_dcpl = static_cast<h5::dcpl_t>( H5P_DEFAULT );
	const static h5::dxpl_t default_dxpl = static_cast<h5::dxpl_t>( H5P_DEFAULT );
	const static h5::lcpl_t default_lcpl = h5::char_encoding{H5T_CSET_UTF8} | h5::create_intermediate_group{1};
	const static h5::fapl_t default_fapl = static_cast<h5::fapl_t>( H5P_DEFAULT );
	const static h5::fcpl_t default_fcpl = static_cast<h5::fcpl_t>( H5P_DEFAULT );
}

#endif

