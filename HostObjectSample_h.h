

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0622 */
/* at Tue Jan 19 11:14:07 2038
 */
/* Compiler settings for HostObjectSample.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.01.0622 
    protocol : all , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */



/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */


#ifndef __HostObjectSample_h_h__
#define __HostObjectSample_h_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IHostObjectSample_FWD_DEFINED__
#define __IHostObjectSample_FWD_DEFINED__
typedef interface IHostObjectSample IHostObjectSample;

#endif 	/* __IHostObjectSample_FWD_DEFINED__ */


#ifndef __HostObjectSample_FWD_DEFINED__
#define __HostObjectSample_FWD_DEFINED__

#ifdef __cplusplus
typedef class HostObjectSample HostObjectSample;
#else
typedef struct HostObjectSample HostObjectSample;
#endif /* __cplusplus */

#endif 	/* __HostObjectSample_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 



#ifndef __HostObjectSampleLibrary_LIBRARY_DEFINED__
#define __HostObjectSampleLibrary_LIBRARY_DEFINED__

/* library HostObjectSampleLibrary */
/* [version][uuid] */ 


EXTERN_C const IID LIBID_HostObjectSampleLibrary;

#ifndef __IHostObjectSample_INTERFACE_DEFINED__
#define __IHostObjectSample_INTERFACE_DEFINED__

/* interface IHostObjectSample */
/* [local][object][uuid] */ 


EXTERN_C const IID IID_IHostObjectSample;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("3a14c9c0-bc3e-453f-a314-4ce4a0ec81d8")
    IHostObjectSample : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE MethodWithParametersAndReturnValue( 
            /* [in] */ BSTR stringParameter,
            /* [in] */ INT integerParameter,
            /* [retval][out] */ BSTR *stringResult) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Property( 
            /* [retval][out] */ BSTR *stringResult) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_Property( 
            /* [in] */ BSTR stringValue) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_IndexedProperty( 
            INT index,
            /* [retval][out] */ BSTR *stringResult) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_IndexedProperty( 
            INT index,
            /* [in] */ BSTR stringValue) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CallCallbackAsynchronously( 
            /* [in] */ IDispatch *callbackParameter) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IHostObjectSampleVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IHostObjectSample * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IHostObjectSample * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IHostObjectSample * This);
        
        HRESULT ( STDMETHODCALLTYPE *MethodWithParametersAndReturnValue )( 
            IHostObjectSample * This,
            /* [in] */ BSTR stringParameter,
            /* [in] */ INT integerParameter,
            /* [retval][out] */ BSTR *stringResult);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Property )( 
            IHostObjectSample * This,
            /* [retval][out] */ BSTR *stringResult);
        
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_Property )( 
            IHostObjectSample * This,
            /* [in] */ BSTR stringValue);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_IndexedProperty )( 
            IHostObjectSample * This,
            INT index,
            /* [retval][out] */ BSTR *stringResult);
        
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_IndexedProperty )( 
            IHostObjectSample * This,
            INT index,
            /* [in] */ BSTR stringValue);
        
        HRESULT ( STDMETHODCALLTYPE *CallCallbackAsynchronously )( 
            IHostObjectSample * This,
            /* [in] */ IDispatch *callbackParameter);
        
        END_INTERFACE
    } IHostObjectSampleVtbl;

    interface IHostObjectSample
    {
        CONST_VTBL struct IHostObjectSampleVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IHostObjectSample_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IHostObjectSample_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IHostObjectSample_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IHostObjectSample_MethodWithParametersAndReturnValue(This,stringParameter,integerParameter,stringResult)	\
    ( (This)->lpVtbl -> MethodWithParametersAndReturnValue(This,stringParameter,integerParameter,stringResult) ) 

#define IHostObjectSample_get_Property(This,stringResult)	\
    ( (This)->lpVtbl -> get_Property(This,stringResult) ) 

#define IHostObjectSample_put_Property(This,stringValue)	\
    ( (This)->lpVtbl -> put_Property(This,stringValue) ) 

#define IHostObjectSample_get_IndexedProperty(This,index,stringResult)	\
    ( (This)->lpVtbl -> get_IndexedProperty(This,index,stringResult) ) 

#define IHostObjectSample_put_IndexedProperty(This,index,stringValue)	\
    ( (This)->lpVtbl -> put_IndexedProperty(This,index,stringValue) ) 

#define IHostObjectSample_CallCallbackAsynchronously(This,callbackParameter)	\
    ( (This)->lpVtbl -> CallCallbackAsynchronously(This,callbackParameter) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IHostObjectSample_INTERFACE_DEFINED__ */


EXTERN_C const CLSID CLSID_HostObjectSample;

#ifdef __cplusplus

class DECLSPEC_UUID("637abc45-11f7-4dde-84b4-317d62a638d3")
HostObjectSample;
#endif
#endif /* __HostObjectSampleLibrary_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


