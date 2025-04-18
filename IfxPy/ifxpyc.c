////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 OpenInformix
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http ://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
/*
+------------------------------------------------------------------------------+
| Authors: Sathyanesh Krishnan, Javier Sagrera, Rohit Pandey, Shilpa S Jadhav  |
|         								       |                               |	
+------------------------------------------------------------------------------+
///////////////////////////////////////////////////////////////////////////////
+------------------------------------------------------------------------------+
| Authors: Manas Dadarkar, Salvador Ledezma, Sushant Koduru,                   |
|   Lynh Nguyen, Kanchana Padmanabhan, Dan Scott, Helmut Tessarek,             |
|   Sam Ruby, Kellen Bombardier, Tony Cairns, Abhigyan Agrawal,                |
|   Tarun Pasrija, Rahul Priyadarshi, Akshay Anand, Saba Kauser ,              |
|   Hemlata Bhatt                                                              |
+------------------------------------------------------------------------------+
*/

#define MODULE_RELEASE "2.0.7"

// #include <Python.h>
#ifdef _DEBUG
#undef _DEBUG
#include <Python.h>
#define _DEBUG
#else
#include <Python.h>
#endif

#include <datetime.h>
#include "ifxpyc.h"

#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// equivalent functions on different platforms
#ifdef _WIN32
#define STRCASECMP stricmp
#else
#define STRCASECMP strcasecmp
#endif

const int _check_i = 1;
#define is_bigendian() ( (*(char*)&_check_i) == 0 )
static PyObject *persistent_list;


// True global resources - no need for thread safety here 
static struct _IfxPy_globals *IfxPy_globals;

#ifdef HAVE_SMARTTRIGGER
// ********** Smart Trigger Start ********************** //

short static gCounter = 0;
IFMX_REGISTER_SMART_TRIGGER  gSmartTriggerRegister[NUM_OF_SMART_TRIGGER_REGISTRATION];
IFMX_OPEN_SMART_TRIGGER      gopenSmartTrigger;
IFMX_JOIN_SMART_TRIGGER      gJoinSmartTrigger;
#endif  // HAVE_SMARTTRIGGER

static PyObject *my_callback[NUM_OF_SMART_TRIGGER_REGISTRATION];

typedef struct {
	char label[129];
	int  sessionID;
	struct storeSessionID *next;
} storeSessionID;

storeSessionID *rootNode = NULL;

#define MakeFunction(a,b) void a##b(const char *outBuf) \
                         { \
							PyObject *arglist = NULL; \
                            arglist = Py_BuildValue("(s)", outBuf); \
							PyObject_CallObject(my_callback[b], arglist); \
							return; \
                         }

//The second parameter of below calls could be one less than NUM_OF_SMART_TRIGGER_REGISTRATION i.e. (NUM_OF_SMART_TRIGGER_REGISTRATION - 1)
MakeFunction(TriggerCallback, 0)
MakeFunction(TriggerCallback, 1)
MakeFunction(TriggerCallback, 2)
MakeFunction(TriggerCallback, 3)
MakeFunction(TriggerCallback, 4)
MakeFunction(TriggerCallback, 5)
MakeFunction(TriggerCallback, 6)
MakeFunction(TriggerCallback, 7)
MakeFunction(TriggerCallback, 8)
MakeFunction(TriggerCallback, 9)

#define PrepareCallbackFunction(a, b)  a##b

// ********** Smart Trigger End ********************** //

SQLRETURN SQLBindFileToParam(
    SQLHSTMT          StatementHandle,           // hstmt 
    SQLUSMALLINT      TargetType,                // ipar 
    SQLSMALLINT       DataType,                  // fSqlType
    SQLCHAR           *FileName,
    SQLSMALLINT       *FileNameLength,
    SQLUINTEGER       *FileOptions,
    SQLSMALLINT       MaxFileNameLength,
    SQLINTEGER        *IndicatorValue)
{
    printf("\n Error: SQLBindFileToParam not supported yet");
    printf("\n Binding a parameter marker in an SQL statement to a file reference will be a new feature");
    return(SQL_ERROR);
}
////////////////////////////////////////////////////////////


static void python_IfxPy_init_globals(struct _IfxPy_globals *IfxPy_globals)
{
    // env handle 
    IfxPy_globals->bin_mode = 1;

    memset(IfxPy_globals->__python_conn_err_msg, 0, DB_MAX_ERR_MSG_LEN);
    memset(IfxPy_globals->__python_stmt_err_msg, 0, DB_MAX_ERR_MSG_LEN);
    memset(IfxPy_globals->__python_conn_err_state, 0, SQL_SQLSTATE_SIZE + 1);
    memset(IfxPy_globals->__python_stmt_err_state, 0, SQL_SQLSTATE_SIZE + 1);
}

char *estrdup(char *data)
{
    size_t len = strlen(data);
    char *dup = ALLOC_N(char, len + 1);

    if (dup == NULL)
    {
        PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
        return NULL;
    }
    strcpy(dup, data);

    return dup;
}

char *estrndup(char *data, int max)
{
    size_t len = strlen(data);
    char *dup;

    if (len > max)
    {
        len = max;
    }

    dup = ALLOC_N(char, len + 1);
    if (dup == NULL)
    {
        PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
        return NULL;
    }
    strcpy(dup, data);

    return dup;
}

char *strtolower(char *data, int max)
{
    while (max--)
    {
        data [max] = tolower(data [max]);
    }

    return data;
}

char *strtoupper(char *data, int max)
{
    while (max--)
    {
        data [max] = toupper(data [max]);
    }

    return data;
}

//    static void _python_IfxPy_free_conn_struct 
static void _python_IfxPy_free_conn_struct(conn_handle *handle)
{
    storeSessionID *tempNode = NULL;
	storeSessionID *delNode = NULL;
    // Disconnect from DB. If stmt is allocated, it is freed automatically 
    if (handle->handle_active && !handle->flag_pconnect)
    {
        if (handle->auto_commit == 0)
        {
            SQLEndTran(SQL_HANDLE_DBC, (SQLHDBC)handle->hdbc, SQL_ROLLBACK);
        }
        SQLDisconnect((SQLHDBC)handle->hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, handle->hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, handle->henv);
		handle->handle_active = 0;
        tempNode = rootNode;
		while (tempNode)
		{
			delNode = tempNode;
			tempNode = tempNode->next;
			free(delNode);
			delNode = NULL;
		}
    }
    Py_TYPE(handle)->tp_free((PyObject*)handle);
}

//    static void _python_IfxPy_free_row_struct 
//static void _python_IfxPy_free_row_struct(row_hash_struct *handle) {
// free(handle);
//}

static void _python_IfxPy_clear_param_cache(stmt_handle *stmt_res)
{
    param_node *temp_ptr, *curr_ptr;

    // Free param cache list 
    curr_ptr = stmt_res->head_cache_list;

    while (curr_ptr != NULL)
    {
        // Decrement refcount on Python handle 
        // NOTE: Py_XDECREF checks NULL value 
        Py_XDECREF(curr_ptr->var_pyvalue);

        // Free Values 
        // NOTE: PyMem_Free checks NULL value 
        PyMem_Free(curr_ptr->varname);
        PyMem_Free(curr_ptr->svalue);
        PyMem_Free(curr_ptr->uvalue);
        PyMem_Free(curr_ptr->date_value);
        PyMem_Free(curr_ptr->time_value);
        PyMem_Free(curr_ptr->ts_value);
        PyMem_Free(curr_ptr->interval_value);

        temp_ptr = curr_ptr;
        curr_ptr = curr_ptr->next;

        PyMem_Free(temp_ptr);
    }

    stmt_res->head_cache_list = NULL;
    stmt_res->num_params = 0;
}

static void _python_IfxPy_free_result_struct(stmt_handle* handle)
{
    int i;
    param_node *curr_ptr = NULL, *prev_ptr = NULL;

    if (handle != NULL)
    {
        _python_IfxPy_clear_param_cache(handle);

        // free row data cache 
        if (handle->row_data)
        {
            for (i = 0; i < handle->num_columns; i++)
            {
                switch (handle->column_info [i].type)
                {
                case SQL_CHAR:
                case SQL_VARCHAR:
                case SQL_LONGVARCHAR:
                case SQL_WCHAR:
                case SQL_WVARCHAR:
                case SQL_BIGINT:
                case SQL_DECIMAL:
                case SQL_NUMERIC:
		case SQL_INFX_RC_SET:
		case SQL_INFX_RC_MULTISET:
		case SQL_INFX_RC_LIST:
		case SQL_INFX_RC_ROW:
	 	case SQL_INFX_RC_COLLECTION:
		case SQL_INFX_UDT_FIXED:
		case SQL_INFX_UDT_VARYING:
                    if (handle->row_data [i].data.str_val != NULL)
                    {
                        PyMem_Del(handle->row_data [i].data.str_val);
                        handle->row_data [i].data.str_val = NULL;
                    }
                    if (handle->row_data [i].data.w_val != NULL)
                    {
                        PyMem_Del(handle->row_data [i].data.w_val);
                        handle->row_data [i].data.w_val = NULL;
                    }
                    break;
                case SQL_TYPE_TIMESTAMP:
                    if (handle->row_data [i].data.ts_val != NULL)
                    {
                        PyMem_Del(handle->row_data [i].data.ts_val);
                        handle->row_data [i].data.ts_val = NULL;
                    }
                    break;
                case SQL_TYPE_DATE:
                    if (handle->row_data [i].data.date_val != NULL)
                    {
                        PyMem_Del(handle->row_data [i].data.date_val);
                        handle->row_data [i].data.date_val = NULL;
                    }
                    break;
                case SQL_TYPE_TIME:
                    if (handle->row_data [i].data.time_val != NULL)
                    {
                        PyMem_Del(handle->row_data [i].data.time_val);
                        handle->row_data [i].data.time_val = NULL;
                    }
                    break;

                case SQL_INTERVAL_DAY:
                case SQL_INTERVAL_HOUR:
                case SQL_INTERVAL_MINUTE:
                case SQL_INTERVAL_SECOND:
                case SQL_INTERVAL_DAY_TO_HOUR:
                case SQL_INTERVAL_DAY_TO_MINUTE:
                case SQL_INTERVAL_DAY_TO_SECOND:
                case SQL_INTERVAL_HOUR_TO_MINUTE:
                case SQL_INTERVAL_HOUR_TO_SECOND:
                case SQL_INTERVAL_MINUTE_TO_SECOND:
                    if (handle->row_data [i].data.interval_val != NULL)
                    {
                        PyMem_Del(handle->row_data [i].data.interval_val);
                        handle->row_data [i].data.interval_val = NULL;
                    }
                    break;
                }
            }
            PyMem_Del(handle->row_data);
            handle->row_data = NULL;
        }

        // free column info cache 
        if (handle->column_info)
        {
            for (i = 0; i < handle->num_columns; i++)
            {
                PyMem_Del(handle->column_info [i].name);
                // Mem free 
                if (handle->column_info [i].mem_alloc)
                {
                    PyMem_Del(handle->column_info [i].mem_alloc);
                }
            }
            PyMem_Del(handle->column_info);
            handle->column_info = NULL;
            handle->num_columns = 0;
        }
    }
}

static stmt_handle *_IfxPy_new_stmt_struct(conn_handle* conn_res)
{
    stmt_handle *stmt_res;

    stmt_res = PyObject_NEW(stmt_handle, &stmt_handleType);
    // memset(stmt_res, 0, sizeof(stmt_handle)); 

    // Initialize stmt resource so parsing assigns updated options if needed 
    stmt_res->hdbc = conn_res->hdbc;
	stmt_res->connhandle = conn_res;
    stmt_res->s_bin_mode = conn_res->c_bin_mode;
    stmt_res->cursor_type = conn_res->c_cursor_type;
    stmt_res->s_case_mode = conn_res->c_case_mode;
    stmt_res->s_use_wchar = conn_res->c_use_wchar;

    stmt_res->head_cache_list = NULL;
    stmt_res->current_node = NULL;

    stmt_res->num_params = 0;
    stmt_res->file_param = 0;

    stmt_res->column_info = NULL;
    stmt_res->num_columns = 0;

    stmt_res->error_recno_tracker = 1;
    stmt_res->errormsg_recno_tracker = 1;

    stmt_res->row_data = NULL;

    return stmt_res;
}

static void _python_IfxPy_free_stmt_struct(stmt_handle *handle)
{
    static int TestingOnly = 0;

    if ( (handle->hstmt  != -1) && (handle->connhandle->handle_active == 1))
    {
        SQLFreeHandle(SQL_HANDLE_STMT, handle->hstmt);
        if (handle)
        {
            _python_IfxPy_free_result_struct(handle);
        }
    }
    Py_TYPE(handle)->tp_free((PyObject*)handle);
}

static void _python_IfxPy_init_error_info(stmt_handle *stmt_res)
{
    stmt_res->error_recno_tracker = 1;
    stmt_res->errormsg_recno_tracker = 1;
}

static void _python_IfxPy_check_sql_errors(SQLHANDLE handle, SQLSMALLINT hType, int rc, int cpy_to_global, char* ret_str, int API, SQLSMALLINT recno)
{
    SQLCHAR msg [SQL_MAX_MESSAGE_LENGTH + 1] = { 0 };
    SQLCHAR sqlstate [SQL_SQLSTATE_SIZE + 1] = { 0 };
    SQLCHAR errMsg [DB_MAX_ERR_MSG_LEN] = { 0 };
    SQLINTEGER sqlcode = 0;
    SQLSMALLINT length = 0;
    char *p = NULL;
    SQLINTEGER rc1 = SQL_SUCCESS;

    memset(errMsg, '\0', DB_MAX_ERR_MSG_LEN);
    memset(msg, '\0', SQL_MAX_MESSAGE_LENGTH + 1);
    rc1 = SQLGetDiagRec(hType, handle, recno, sqlstate, &sqlcode, msg,
                        SQL_MAX_MESSAGE_LENGTH + 1, &length);
    if (rc1 == SQL_SUCCESS)
    {
        while ((p = strchr((char *)msg, '\n')))
        {
            *p = '\0';
        }
        sprintf((char*)errMsg, "%s SQLCODE=%d", (char*)msg, (int)sqlcode);
        if (cpy_to_global != 0 && rc != 1)
        {
            PyErr_SetString(PyExc_Exception, (char *)errMsg);
        }

        switch (rc)
        {
        case SQL_ERROR:
            // Need to copy the error msg and sqlstate into the symbol Table
            // to cache these results 
            if (cpy_to_global)
            {
                switch (hType)
                {
                case SQL_HANDLE_DBC:
                    strncpy(IFX_G(__python_conn_err_state),
                        (char*)sqlstate, SQL_SQLSTATE_SIZE + 1);
                    strncpy(IFX_G(__python_conn_err_msg),
                        (char*)errMsg, DB_MAX_ERR_MSG_LEN);
                    break;

                case SQL_HANDLE_STMT:
                    strncpy(IFX_G(__python_stmt_err_state),
                        (char*)sqlstate, SQL_SQLSTATE_SIZE + 1);
                    strncpy(IFX_G(__python_stmt_err_msg),
                        (char*)errMsg, DB_MAX_ERR_MSG_LEN);
                    break;
                }
            }
            /* This call was made from IfxPy_errmsg or IfxPy_error or IfxPy_warn */
            /* Check for error and return */
            switch (API)
            {
            case IDS_ERR:
                if (ret_str != NULL)
                {
                    strncpy(ret_str, (char*)sqlstate, SQL_SQLSTATE_SIZE + 1);
                }
                return;
            case IDS_ERRMSG:
                if (ret_str != NULL)
                {
                    strncpy(ret_str, (char*)errMsg, DB_MAX_ERR_MSG_LEN);
                }
                return;
            default:
                break;
            }
            break;
        case SQL_SUCCESS_WITH_INFO:
            /* Need to copy the warning msg and sqlstate into the symbol Table */
            /* to cache these results */
            if (cpy_to_global)
            {
                switch (hType)
                {
                case SQL_HANDLE_DBC:
                    strncpy(IFX_G(__python_conn_warn_state),
                        (char*)sqlstate, SQL_SQLSTATE_SIZE + 1);
                    strncpy(IFX_G(__python_conn_warn_msg),
                        (char*)errMsg, DB_MAX_ERR_MSG_LEN);
                    break;

                case SQL_HANDLE_STMT:
                    strncpy(IFX_G(__python_stmt_warn_state),
                        (char*)sqlstate, SQL_SQLSTATE_SIZE + 1);
                    strncpy(IFX_G(__python_stmt_warn_msg),
                        (char*)errMsg, DB_MAX_ERR_MSG_LEN);
                    break;
                }
            }
            /* This call was made from IfxPy_errmsg or IfxPy_error or IfxPy_warn */
            /* Check for error and return */
            if ((API == IDS_WARNMSG) && (ret_str != NULL))
            {
                strncpy(ret_str, (char*)errMsg, DB_MAX_ERR_MSG_LEN);
            }
            return;
        default:
            break;
        }
    }
}

#ifdef HAVE_SMARTTRIGGER
static PyObject *IfxPy_join_smart_trigger_session(PyObject *self, PyObject *args)
{
	PyObject *result = NULL;
	PyObject *py_conn_res = NULL;
	PyObject *callbackObj = NULL;
	PyObject *sessionIDObj = NULL;
	PyObject *ControlBackToApplicationObj = NULL;
	int sessionID = 0;
	char *return_str = NULL;
	conn_handle *conn_res;
	int rc;
	SQLHANDLE tmpHstmt;
	BOOL ControlBackToApplication = FALSE;


	if (PyArg_ParseTuple(args, "OOOO", &py_conn_res, &callbackObj, &sessionIDObj, &ControlBackToApplicationObj))
	{
		if (!PyCallable_Check(callbackObj))
		{
			PyErr_SetString(PyExc_TypeError, "parameter must be callable");
			return NULL;
		}
		Py_XINCREF(callbackObj);         /* Add a reference to new callback */
		Py_XDECREF(my_callback[0]);  /* Dispose of previous callback */
		my_callback[0] = callbackObj;       /* Remember new callback */
										 /* Boilerplate to return "None" */
	    Py_INCREF(Py_None);
		result = Py_None;

		if ( !PyLong_Check(sessionIDObj) )
		{
			PyErr_SetString(PyExc_TypeError, "Error : Session ID object should be of type int/long");
			return NULL;
		}

		if ( !PyBool_Check(ControlBackToApplicationObj) )
		{
			PyErr_SetString(PyExc_TypeError, "Error : Input object should be of type Bool/Boolean");
			return NULL;
		}

		if (!NIL_P(py_conn_res))
		{
			if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
			{
				PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
				return NULL;
			}
			else
			{
				conn_res = (conn_handle *)py_conn_res;
			}

			return_str = ALLOC_N(char, DB_MAX_ERR_MSG_LEN);
			if (return_str == NULL)
			{
				PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
				return NULL;
			}

			memset(return_str, 0, DB_MAX_ERR_MSG_LEN);
			_python_IfxPy_clear_stmt_err_cache();
			
			rc = SQLAllocHandle(SQL_HANDLE_STMT, conn_res->hdbc, &tmpHstmt);
			if (rc == SQL_ERROR)
			{
				_python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1, NULL, -1, 1);
				PyMem_Del(return_str);
				return NULL;
			}

			if (ControlBackToApplicationObj == Py_False)
				ControlBackToApplication = FALSE;
			else
				ControlBackToApplication = TRUE;
			
			sessionID = PyLong_AsLong(sessionIDObj);
			printf("\n Session ID to be joined = %d\n", sessionID);

			gJoinSmartTrigger.callback = TriggerCallback0; // TriggerCallback1;
			gJoinSmartTrigger.joinSessionID = &sessionID;
			gJoinSmartTrigger.ControlBackToApplication = &ControlBackToApplication;

			rc = SQLSetStmtAttr((SQLHSTMT)tmpHstmt, PY_IFMX_JOIN_SMART_TRIGGER, &gJoinSmartTrigger, SQL_NTS);
			if (rc == SQL_ERROR)
			{
				_python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1, NULL, -1, 1);
				PyMem_Del(return_str);
				return NULL;
			}
			SQLFreeHandle(SQL_HANDLE_STMT, tmpHstmt);
			result = Py_None;
		}
	}
	return result;
}

static PyObject *IfxPy_delete_smart_trigger_session(PyObject *self, PyObject *args)
{
	PyObject *result = NULL;
	PyObject *py_conn_res = NULL;
	PyObject *sessionIDObj = NULL;
	int sessionID = 0;
	char *return_str = NULL;
	conn_handle *conn_res;
	int rc;
	SQLHANDLE tmpHstmt;


	if (PyArg_ParseTuple(args, "OO", &py_conn_res, &sessionIDObj))
	{
		if (!PyLong_Check(sessionIDObj))
		{
			PyErr_SetString(PyExc_TypeError, "Error : Session ID object should be of type int/long");
			return NULL;
		}

		if (!NIL_P(py_conn_res))
		{
			if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
			{
				PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
				return NULL;
			}
			else
			{
				conn_res = (conn_handle *)py_conn_res;
			}

			return_str = ALLOC_N(char, DB_MAX_ERR_MSG_LEN);
			if (return_str == NULL)
			{
				PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
				return NULL;
			}

			memset(return_str, 0, DB_MAX_ERR_MSG_LEN);
			_python_IfxPy_clear_stmt_err_cache();

			rc = SQLAllocHandle(SQL_HANDLE_STMT, conn_res->hdbc, &tmpHstmt);
			if (rc == SQL_ERROR)
			{
				_python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1, NULL, -1, 1);
				PyMem_Del(return_str);
				return NULL;
			}

			sessionID = PyLong_AsLong(sessionIDObj);
			printf("\n Session ID to be deleted = %d\n", sessionID);
			rc = SQLSetStmtAttr((SQLHSTMT)tmpHstmt, PY_IFMX_DELETE_SMART_TRIGGER, &sessionID, SQL_NTS);
			if (rc == SQL_ERROR)
			{
				_python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1, NULL, -1, 1);
				PyMem_Del(return_str);
				return NULL;
			}
			SQLFreeHandle(SQL_HANDLE_STMT, tmpHstmt);
			result = Py_None;
		}
	}
	return result;
}

static PyObject *IfxPy_get_smart_trigger_sessionID(PyObject *self, PyObject *args)
{
	PyObject *result = NULL;
	PyObject *sessionIDObj = NULL;
	char      *SessionIDString = NULL;
	storeSessionID *tempNode = NULL;
	BOOL found = FALSE;

	if (PyArg_ParseTuple(args, "O", &sessionIDObj))
	{
		if (sessionIDObj != NULL && sessionIDObj != Py_None)
		{
			if (PyString_Check(sessionIDObj) || PyUnicode_Check(sessionIDObj))
			{
				sessionIDObj = PyUnicode_FromObject(sessionIDObj);
			}
			else
			{
				PyErr_SetString(PyExc_Exception, "Argument must be a string or unicode");
				return NULL;
			}
			sessionIDObj = PyUnicode_AsASCIIString(sessionIDObj);
			if (sessionIDObj == NULL)
			{
				PyErr_SetString(PyExc_Exception, "Argument must be a string or unicode");
				return NULL;
			}
			SessionIDString = PyBytes_AsString(sessionIDObj);
			printf("\n SessionIDString : %s\n", SessionIDString);
			if (!rootNode)
			{
				PyErr_SetString(PyExc_Exception, "Unexpected null pointer found!!");
				return NULL;
			}
			tempNode = rootNode;
			while (tempNode)
			{
				if (strcmp(SessionIDString, tempNode->label) == 0)
				{
					result = PyLong_FromLong(tempNode->sessionID);
					found = TRUE;
					break;
				}
				tempNode = tempNode->next;
			}
			if (found == FALSE)
			{
				PyErr_SetString(PyExc_Exception, "The associated session ID not found for the unique string provided.");
				return NULL;
			}
		}
		else
		{
			PyErr_SetString(PyExc_Exception, "Argument is NULL or Py_None. It should be non-Null and should contain proper value of length not more than 128 bytes.");
			return NULL;
		}
	}
	return result;
}

static PyObject *IfxPy_open_smart_trigger(PyObject *self, PyObject *args)
{
	PyObject *result = NULL;
	PyObject *py_conn_res = NULL;
	PyObject *isDetachableObj = NULL;
	PyObject *timeOutObj = NULL;
	PyObject *maxRecsPerReadObj = NULL;
	PyObject *maxPendingOperationsObj = NULL;
	PyObject *uniqueSessionIDStringObj = NULL;
	PyObject *reservedObj = NULL;
	conn_handle *conn_res;
	int rc;
	char     *return_str = NULL; // This variable is used by _python_IfxPy_check_sql_errors to return err strings
	BOOL     isDetachable; //Default False
	int      timeOut; //Default 120 secs
	short    maxRecsPerRead; //Default 1
	int      maxPendingOperations; //Default 0
	char     *uniqueSessionIDString;
	char     reserved[16];
	int      fileDesc = 0;
	int      getSesId = 0;
	SQLHANDLE tmpHstmt;
	storeSessionID *tempNode = NULL;
    char error [DB_MAX_ERR_MSG_LEN];

	if (PyArg_ParseTuple(args, "OOOOOO|O", &py_conn_res, &uniqueSessionIDStringObj, &isDetachableObj, &timeOutObj, &maxRecsPerReadObj, &maxPendingOperationsObj, &reservedObj)) 
	{
		if ( !PyBool_Check(isDetachableObj) || !PyLong_Check(timeOutObj) || !PyLong_Check(maxRecsPerReadObj) || !PyLong_Check(maxPendingOperationsObj) ) 
		{
			PyErr_SetString(PyExc_TypeError, "One of the paremeters is not of correct type. Recheck and try again.");
			return NULL;
		}

		if (uniqueSessionIDStringObj != NULL && uniqueSessionIDStringObj != Py_None)
		{
			if (PyString_Check(uniqueSessionIDStringObj) || PyUnicode_Check(uniqueSessionIDStringObj))
			{
				uniqueSessionIDStringObj = PyUnicode_FromObject(uniqueSessionIDStringObj);
			}
			else
			{
				PyErr_SetString(PyExc_Exception, "Argument must be a string or unicode");
				return NULL;
			}
			uniqueSessionIDStringObj = PyUnicode_AsASCIIString(uniqueSessionIDStringObj);
			if (uniqueSessionIDStringObj == NULL)
			{
				PyErr_SetString(PyExc_Exception, "Argument must be a string or unicode");
				return NULL;
			}
			uniqueSessionIDString = PyBytes_AsString(uniqueSessionIDStringObj);
		}
		else
		{
			PyErr_SetString(PyExc_Exception, "Argument is NULL or Py_None. It should be non-Null and should contain proper value of length not more than 128 bytes.");
			return NULL;
		}
		if (!NIL_P(py_conn_res))
		{
			if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
			{
				PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
				return NULL;
			}
			else
			{
				conn_res = (conn_handle *)py_conn_res;
			}

			return_str = ALLOC_N(char, DB_MAX_ERR_MSG_LEN);
			if (return_str == NULL)
			{
				PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
				return NULL;
			}

			memset(return_str, 0, DB_MAX_ERR_MSG_LEN);
			_python_IfxPy_clear_stmt_err_cache();

			rc = SQLAllocHandle(SQL_HANDLE_STMT, conn_res->hdbc, &tmpHstmt);
			if (rc == SQL_ERROR)
			{
				_python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1, NULL, -1, 1);
				PyMem_Del(return_str);
				return NULL;
			}
		
			if (isDetachableObj == Py_False)
				isDetachable = FALSE;
			else
				isDetachable = TRUE;

			timeOut = PyLong_AsLong(timeOutObj);
			maxRecsPerRead = (short)PyLong_AsLong(maxRecsPerReadObj);
			maxPendingOperations = PyLong_AsLong(maxPendingOperationsObj);

			gopenSmartTrigger.isDetachable = &isDetachable;
			gopenSmartTrigger.maxPendingOperations = &maxPendingOperations;
			gopenSmartTrigger.maxRecsPerRead = &maxRecsPerRead;
			gopenSmartTrigger.timeOut = &timeOut;

			rc = SQLSetStmtAttr((SQLHSTMT)tmpHstmt, PY_IFMX_OPEN_SMART_TRIGGER, &gopenSmartTrigger, SQL_NTS);
			if (rc == SQL_ERROR)
			{
				_python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1, NULL, -1, 1);
				PyMem_Del(return_str);
                // Problem is here
				return NULL;
                //return rc;
			}

			rc = SQLGetStmtAttr((SQLHSTMT)tmpHstmt, PY_IFMX_GET_LO_FILE_DESC_SMART_TRIGGER, (int *)&fileDesc, SQL_NTS, NULL);
			if (rc == SQL_ERROR)
			{
				_python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1, NULL, -1, 1);
				PyMem_Del(return_str);
				SQLFreeHandle(SQL_HANDLE_STMT, tmpHstmt);
				return NULL;
			}
			result = PyLong_FromLong(fileDesc);

			rc = SQLGetStmtAttr((SQLHSTMT)tmpHstmt, PY_IFMX_GET_SESSION_ID_SMART_TRIGGER, (int *)&getSesId, SQL_NTS, NULL);
			if (rc == SQL_ERROR)
			{
				_python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1, NULL, -1, 1);
				PyMem_Del(return_str);
				SQLFreeHandle(SQL_HANDLE_STMT, tmpHstmt);
				return NULL;
			}
			if (!rootNode)
			{
				rootNode = (storeSessionID *)malloc(sizeof(storeSessionID));
				if (!rootNode)
				{
					_python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1, NULL, -1, 1);
					PyMem_Del(return_str);
					SQLFreeHandle(SQL_HANDLE_STMT, tmpHstmt);
					return NULL;
				}
				strcpy(rootNode->label, uniqueSessionIDString);
				rootNode->sessionID = getSesId;
				rootNode->next = NULL;
			}
			else
			{
				tempNode = (storeSessionID *)malloc(sizeof(storeSessionID));
				if (!rootNode)
				{
					_python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1, NULL, -1, 1);
					PyMem_Del(return_str);
					SQLFreeHandle(SQL_HANDLE_STMT, tmpHstmt);
					return NULL;
				}
				rootNode->next = tempNode;
				strcpy(tempNode->label, uniqueSessionIDString);
				tempNode->sessionID = getSesId;
				tempNode->next = NULL;
            }		
			SQLFreeHandle(SQL_HANDLE_STMT, tmpHstmt);
		}
	}
	return result;
}

static PyObject *_python_IfxPy_register_smart_trigger_helper(PyObject *self, PyObject *args, BOOL isLoop)
{
	PyObject *result = Py_None;
	PyObject *py_conn_res = NULL;
	PyObject *callbackObj = NULL;
	PyObject *loFileDescriptorObj = NULL;
	PyObject *tableNameObj = NULL;
	PyObject *ownerNameObj = NULL;
	PyObject *dbNameObj = NULL;
	PyObject *sqlQueryObj = NULL;
	PyObject *labelObj = NULL;
	PyObject *isDeregisterObj = NULL;
	PyObject *ControlBackToApplicationObj = NULL;
	PyObject *reservedObj = NULL;
	conn_handle *conn_res;
	int rc;
	char    *return_str = NULL; // This variable is used by _python_IfxPy_check_sql_errors to return err strings
	int     loFileDescriptor;
	SQLWCHAR    *tableName = NULL;
	SQLWCHAR    *ownerName = NULL;
	SQLWCHAR    *dbName = NULL;
	SQLWCHAR    *sqlQuery = NULL;
	SQLWCHAR    *label = NULL;
	BOOL    isDeregister;
	BOOL    ControlBackToApplication;
	SQLWCHAR    reserved[16];
	SQLINTEGER	    dummy = 0;
	SQLHANDLE tmpHstmt;
	short Counter = 0;
	Py_ssize_t retLength = 0;
	Py_ssize_t objectSize = 0;

	if (PyArg_ParseTuple(args, "OOOOOOOOOO|O", &py_conn_res, &callbackObj, &loFileDescriptorObj, &tableNameObj, &ownerNameObj, &dbNameObj, &sqlQueryObj, &labelObj, &isDeregisterObj, &ControlBackToApplicationObj, &reservedObj))
	{
		if (!PyCallable_Check(callbackObj))
		{
			PyErr_SetString(PyExc_TypeError, "parameter must be callable");
			return NULL;
		}
		
		Counter = gCounter;
		gCounter++;
		
		if (Counter >= NUM_OF_SMART_TRIGGER_REGISTRATION)
		{
			PyErr_SetString(PyExc_Exception, "Can't register more than 10 events. Increase the counter (and modify associated code) to suit your need in the Python code.");
			return NULL;
		}

		Py_XINCREF(callbackObj);         /* Add a reference to new callback */
		Py_XDECREF(my_callback[Counter]);  /* Dispose of previous callback */
		my_callback[Counter] = callbackObj;       /* Remember new callback */
												  /* Boilerplate to return "None" */
		Py_INCREF(Py_None);
		result = Py_None;

		if (!PyLong_Check(loFileDescriptorObj))
		{
			PyErr_SetString(PyExc_Exception, "File Description parametr should be integer");
			return NULL;
		}
		if (!PyBool_Check(isDeregisterObj) || !PyBool_Check(ControlBackToApplicationObj))
		{
			PyErr_SetString(PyExc_TypeError, "One of the paremeters is not of boolean type. Recheck and try again.");
			return NULL;
		}
		if (tableNameObj != NULL && tableNameObj != Py_None)
		{
			if (PyString_Check(tableNameObj) || PyUnicode_Check(tableNameObj))
			{
				tableNameObj = PyUnicode_FromObject(tableNameObj);
			}
			else
			{
				PyErr_SetString(PyExc_Exception, "statement must be a string or unicode");
				return NULL;
			}

			objectSize = PyUnicode_GET_LENGTH(tableNameObj);
			tableName = (SQLWCHAR *)malloc((int)objectSize*sizeof(SQLWCHAR) + sizeof(SQLWCHAR));
			if (tableName == NULL)
			{
				PyErr_SetString(PyExc_Exception, "Table name : Memory allocation failed.");
				return NULL;
			}
			retLength = PyUnicode_AsWideChar(tableNameObj, tableName, objectSize);
			if ((int)retLength < 0)
			{
				PyErr_SetString(PyExc_Exception, "Unicode to wide conversion failed for Table name object.");
				return NULL;
			}
		}
		if (ownerNameObj != NULL && ownerNameObj != Py_None)
		{
			if (PyString_Check(ownerNameObj) || PyUnicode_Check(ownerNameObj))
			{
				ownerNameObj = PyUnicode_FromObject(ownerNameObj);
			}
			else
			{
				PyErr_SetString(PyExc_Exception, "statement must be a string or unicode");
				return NULL;
			}
			objectSize = PyUnicode_GET_LENGTH(ownerNameObj);
			ownerName = (SQLWCHAR *)malloc((int)objectSize * sizeof(SQLWCHAR) + sizeof(SQLWCHAR));
			if (ownerName == NULL)
			{
				PyErr_SetString(PyExc_Exception, "Owner name : Memory allocation failed.");
				return NULL;
			}
			retLength = PyUnicode_AsWideChar(ownerNameObj, ownerName, objectSize);
			if ((int)retLength < 0)
			{
				PyErr_SetString(PyExc_Exception, "Unicode to wide conversion failed for Owner name object.");
				return NULL;
			}
		}
		if (dbNameObj != NULL && dbNameObj != Py_None)
		{
			if (PyString_Check(dbNameObj) || PyUnicode_Check(dbNameObj))
			{
				dbNameObj = PyUnicode_FromObject(dbNameObj);
			}
			else
			{
				PyErr_SetString(PyExc_Exception, "statement must be a string or unicode");
				return NULL;
			}
			objectSize = PyUnicode_GET_LENGTH(dbNameObj);
			dbName = (SQLWCHAR *)malloc((int)objectSize * sizeof(SQLWCHAR) + sizeof(SQLWCHAR));
			if (dbName == NULL)
			{
				PyErr_SetString(PyExc_Exception, "DB name : Memory allocation failed.");
				return NULL;
			}
			retLength = PyUnicode_AsWideChar(dbNameObj, dbName, objectSize);
			if ((int)retLength < 0)
			{
				PyErr_SetString(PyExc_Exception, "Unicode to wide conversion failed for Database name object.");
				return NULL;
			}
		}
		if (sqlQueryObj != NULL && sqlQueryObj != Py_None)
		{
			if (PyString_Check(sqlQueryObj) || PyUnicode_Check(sqlQueryObj))
			{
				sqlQueryObj = PyUnicode_FromObject(sqlQueryObj);
			}
			else
			{
				PyErr_SetString(PyExc_Exception, "statement must be a string or unicode");
				return NULL;
			}
			objectSize = PyUnicode_GET_LENGTH(sqlQueryObj);
			sqlQuery = (SQLWCHAR *)malloc((int)objectSize * sizeof(SQLWCHAR) + sizeof(SQLWCHAR));
			if (sqlQuery == NULL)
			{
				PyErr_SetString(PyExc_Exception, "SQL Query : Memory allocation failed.");
				return NULL;
			}
			retLength = PyUnicode_AsWideChar(sqlQueryObj, sqlQuery, objectSize);
			if ((int)retLength < 0)
			{
				PyErr_SetString(PyExc_Exception, "Unicode to wide conversion failed for SQL Query object.");
				return NULL;
			}
		}
		if (labelObj != NULL && labelObj != Py_None)
		{
			if (PyString_Check(labelObj) || PyUnicode_Check(labelObj))
			{
				labelObj = PyUnicode_FromObject(labelObj);
			}
			else
			{
				PyErr_SetString(PyExc_Exception, "Statement must be a string or unicode");
				return NULL;
			}
			objectSize = PyUnicode_GET_LENGTH(labelObj);
			label = (SQLWCHAR *)malloc((int)objectSize * sizeof(SQLWCHAR) + sizeof(SQLWCHAR));
			if (label == NULL)
			{
				PyErr_SetString(PyExc_Exception, "Label : Memory allocation failed.");
				return NULL;
			}
			retLength = PyUnicode_AsWideChar(labelObj, label, objectSize);
			if ((int)retLength < 0)
			{
				PyErr_SetString(PyExc_Exception, "Unicode to wide conversion failed for Label object.");
				return NULL;
			}
		}

		if (isDeregisterObj == Py_False)
			isDeregister = FALSE;
		else
			isDeregister = TRUE;

		if (ControlBackToApplicationObj == Py_False)
			ControlBackToApplication = FALSE;
		else
			ControlBackToApplication = TRUE;

		if (!NIL_P(py_conn_res))
		{
			if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
			{
				PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
				return NULL;
			}
			else
			{
				conn_res = (conn_handle *)py_conn_res;
			}

			return_str = ALLOC_N(char, DB_MAX_ERR_MSG_LEN);
			if (return_str == NULL)
			{
				PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
				return NULL;
			}

			memset(return_str, 0, DB_MAX_ERR_MSG_LEN);
			_python_IfxPy_clear_stmt_err_cache();

			rc = SQLAllocHandle(SQL_HANDLE_STMT, conn_res->hdbc, &tmpHstmt);
			if (rc == SQL_ERROR)
			{
				_python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1, NULL, -1, 1);
				PyMem_Del(return_str);
				return NULL;
			}

			switch (Counter)
			{
			case 0:
				gSmartTriggerRegister[Counter].callback = PrepareCallbackFunction(TriggerCallback, 0);
				break;
			case 1:
				gSmartTriggerRegister[Counter].callback = PrepareCallbackFunction(TriggerCallback, 1);
				break;
			case 2:
				gSmartTriggerRegister[Counter].callback = PrepareCallbackFunction(TriggerCallback, 2);
				break;
			case 3:
				gSmartTriggerRegister[Counter].callback = PrepareCallbackFunction(TriggerCallback, 3);
				break;
			case 4:
				gSmartTriggerRegister[Counter].callback = PrepareCallbackFunction(TriggerCallback, 4);
				break;
			case 5:
				gSmartTriggerRegister[Counter].callback = PrepareCallbackFunction(TriggerCallback, 5);
				break;
			case 6:
				gSmartTriggerRegister[Counter].callback = PrepareCallbackFunction(TriggerCallback, 6);
				break;
			case 7:
				gSmartTriggerRegister[Counter].callback = PrepareCallbackFunction(TriggerCallback, 7);
				break;
			case 8:
				gSmartTriggerRegister[Counter].callback = PrepareCallbackFunction(TriggerCallback, 8);
				break;
			case 9:
				gSmartTriggerRegister[Counter].callback = PrepareCallbackFunction(TriggerCallback, 9);
				break;
			default:
				PyErr_SetString(PyExc_Exception, "Can't register more than 10 events. Increase the counter (and modify associated code) to suit your need in the Python code.");
				return NULL;
			}

			loFileDescriptor = PyLong_AsLong(loFileDescriptorObj);
			gSmartTriggerRegister[Counter].loFileDescriptor = &loFileDescriptor;
			gSmartTriggerRegister[Counter].tableName = tableName;
			gSmartTriggerRegister[Counter].ownerName = ownerName;
			gSmartTriggerRegister[Counter].dbName = dbName;
			gSmartTriggerRegister[Counter].sqlQuery = sqlQuery;
			gSmartTriggerRegister[Counter].label = label;
			gSmartTriggerRegister[Counter].isDeregister = &isDeregister;
			gSmartTriggerRegister[Counter].ControlBackToApplication = &ControlBackToApplication;
			
			rc = SQLSetStmtAttrW((SQLHSTMT)tmpHstmt, PY_IFMX_REGISTER_SMART_TRIGGER, (void *)&gSmartTriggerRegister[Counter], SQL_IS_POINTER);
			if (rc == SQL_ERROR)
			{
				_python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1, NULL, -1, 1);
				PyMem_Del(return_str);
				SQLFreeHandle(SQL_HANDLE_STMT, tmpHstmt);
				return NULL;
			}

			if (isLoop == TRUE)
			{
				rc = SQLGetStmtAttr((SQLHSTMT)tmpHstmt, PY_IFMX_GET_DATA_SMART_TRIGGER_LOOP, (void *)&dummy, SQL_NTS, NULL);
			}
			else
			{
				rc = SQLGetStmtAttr((SQLHSTMT)tmpHstmt, PY_IFMX_GET_DATA_SMART_TRIGGER_NO_LOOP, (void *)&dummy, SQL_NTS, NULL);
			}
			if (rc == SQL_ERROR)
			{
				_python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1, NULL, -1, 1);
				PyMem_Del(return_str);
				SQLFreeHandle(SQL_HANDLE_STMT, tmpHstmt);
				return NULL;
			}
			result = Py_None; // PyLong_FromLong(fileDesc);
			
			free(tableName);
			free(ownerName);
			free(dbName);
			free(sqlQuery);
			free(label);

			SQLFreeHandle(SQL_HANDLE_STMT, tmpHstmt);
		}
	}
	return result;
}

static PyObject *IfxPy_register_smart_trigger_no_loop(PyObject *self, PyObject *args)
{
	return _python_IfxPy_register_smart_trigger_helper(self, args, FALSE);
}

static PyObject *IfxPy_register_smart_trigger_loop(PyObject *self, PyObject *args)
{
	return _python_IfxPy_register_smart_trigger_helper(self, args, TRUE);
}
#endif  // HAVE_SMARTTRIGGER

/*    static int _python_IfxPy_assign_options( void *handle, int type, long opt_key, PyObject *data ) */
static int _python_IfxPy_assign_options(void *handle, int type, long opt_key, PyObject *data)
{
    int rc = 0;
    long option_num = 0;
    SQLWCHAR *option_str = NULL;
    int isNewBuffer;

    /* First check to see if it is a non-cli attribut */
    if (opt_key == ATTR_CASE)
    {
        option_num = NUM2LONG(data);
        if (type == SQL_HANDLE_STMT)
        {
            switch (option_num)
            {
            case CASE_LOWER:
                ((stmt_handle*)handle)->s_case_mode = CASE_LOWER;
                break;
            case CASE_UPPER:
                ((stmt_handle*)handle)->s_case_mode = CASE_UPPER;
                break;
            case CASE_NATURAL:
                ((stmt_handle*)handle)->s_case_mode = CASE_NATURAL;
                break;
            default:
                PyErr_SetString(PyExc_Exception, "ATTR_CASE attribute must be one of CASE_LOWER, CASE_UPPER, or CASE_NATURAL");
                return -1;
            }
        }
        else if (type == SQL_HANDLE_DBC)
        {
            switch (option_num)
            {
            case CASE_LOWER:
                ((conn_handle*)handle)->c_case_mode = CASE_LOWER;
                break;
            case CASE_UPPER:
                ((conn_handle*)handle)->c_case_mode = CASE_UPPER;
                break;
            case CASE_NATURAL:
                ((conn_handle*)handle)->c_case_mode = CASE_NATURAL;
                break;
            default:
                PyErr_SetString(PyExc_Exception, "ATTR_CASE attribute must be one of CASE_LOWER, CASE_UPPER, or CASE_NATURAL");
                return -1;
            }
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "Connection or statement handle must be passed in.");
            return -1;
        }
    }
    else if (opt_key == USE_WCHAR)
    {
        option_num = NUM2LONG(data);
        if (type == SQL_HANDLE_STMT)
        {
            switch (option_num)
            {
            case WCHAR_YES:
                ((stmt_handle*)handle)->s_use_wchar = WCHAR_YES;
                break;
            case WCHAR_NO:
                ((stmt_handle*)handle)->s_use_wchar = WCHAR_NO;
                break;
            default:
                PyErr_SetString(PyExc_Exception, "USE_WCHAR attribute must be one of WCHAR_YES or WCHAR_NO");
                return -1;
            }
        }
        else if (type == SQL_HANDLE_DBC)
        {
            switch (option_num)
            {
            case WCHAR_YES:
                ((conn_handle*)handle)->c_use_wchar = WCHAR_YES;
                break;
            case WCHAR_NO:
                ((conn_handle*)handle)->c_use_wchar = WCHAR_NO;
                break;
            default:
                PyErr_SetString(PyExc_Exception, "USE_WCHAR attribute must be one of WCHAR_YES or WCHAR_NO");
                return -1;
            }
        }
    }
    else if (type == SQL_HANDLE_STMT)
    {
        if (PyString_Check(data) || PyUnicode_Check(data))
        {
            data = PyUnicode_FromObject(data);
            option_str = getUnicodeDataAsSQLWCHAR(data, &isNewBuffer);
            rc = SQLSetStmtAttrW((SQLHSTMT)((stmt_handle *)handle)->hstmt, opt_key, (SQLPOINTER)option_str, SQL_IS_INTEGER);
            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)((stmt_handle *)handle)->hstmt, SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
            }
            if (isNewBuffer)
                PyMem_Del(option_str);

        }
        else
        {
            option_num = NUM2LONG(data);
            if (opt_key == SQL_ATTR_AUTOCOMMIT && option_num == SQL_AUTOCOMMIT_OFF)
            {
                ((conn_handle*)handle)->auto_commit = 0;
            }
            else if (opt_key == SQL_ATTR_AUTOCOMMIT && option_num == SQL_AUTOCOMMIT_ON)
            {
                ((conn_handle*)handle)->auto_commit = 1;
            }

            rc = SQLSetStmtAttr((SQLHSTMT)((stmt_handle *)handle)->hstmt, opt_key, (SQLPOINTER)((SQLLEN)option_num), SQL_IS_INTEGER);
            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)((stmt_handle *)handle)->hstmt, SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
            }
        }
    }
    else if (type == SQL_HANDLE_DBC)
    {
        if (PyString_Check(data) || PyUnicode_Check(data))
        {
            data = PyUnicode_FromObject(data);
            option_str = getUnicodeDataAsSQLWCHAR(data, &isNewBuffer);
            rc = SQLSetConnectAttrW((SQLHSTMT)((conn_handle*)handle)->hdbc, opt_key, (SQLPOINTER)option_str, SQL_NTS);
            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)((stmt_handle *)handle)->hstmt, SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
            }
            if (isNewBuffer)
                PyMem_Del(option_str);

        }
        else
        {
            option_num = NUM2LONG(data);
            if (opt_key == SQL_ATTR_AUTOCOMMIT && option_num == SQL_AUTOCOMMIT_OFF)
            {
                ((conn_handle*)handle)->auto_commit = 0;
            }
            else if (opt_key == SQL_ATTR_AUTOCOMMIT && option_num == SQL_AUTOCOMMIT_ON)
            {
                ((conn_handle*)handle)->auto_commit = 1;
            }

            rc = SQLSetConnectAttrW((SQLHSTMT)((conn_handle*)handle)->hdbc, opt_key, (SQLPOINTER)((SQLLEN)option_num), SQL_IS_INTEGER);
            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)((stmt_handle *)handle)->hstmt, SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
            }
        }
    }
    else
    {
        PyErr_SetString(PyExc_Exception, "Connection or statement handle must be passed in.");
        return -1;
    }
    return 0;
}

/*    static int _python_IfxPy_parse_options( PyObject *options, int type, void *handle)*/
static int _python_IfxPy_parse_options(PyObject *options, int type, void *handle)
{
    Py_ssize_t numOpts = 0, i = 0;
    PyObject *keys = NULL;
    PyObject *key = NULL; /* Holds the Option Index Key */
    PyObject *data = NULL;
    PyObject *tc_pass = NULL;
    int rc = 0;

    if (!NIL_P(options))
    {
        keys = PyDict_Keys(options);
        numOpts = PyList_Size(keys);

        for (i = 0; i < numOpts; i++)
        {
            key = PyList_GetItem(keys, i);
            data = PyDict_GetItem(options, key);


            /* Assign options to handle. */
            /* Sets the options in the handle with CLI/ODBC calls */
            rc = _python_IfxPy_assign_options(handle, type, NUM2LONG(key), data);

            if (rc)
                return SQL_ERROR;
        }

        if (rc)
            return SQL_ERROR;
    }
    return SQL_SUCCESS;
}

//  static int _python_IfxPy_get_result_set_info(stmt_handle *stmt_res)
// initialize the result set information of each column. This must be done once
static int _python_IfxPy_get_result_set_info(stmt_handle *stmt_res)
{
    int rc = -1, i;
    SQLSMALLINT nResultCols = 0, name_length;
    SQLCHAR tmp_name [BUFSIZ];

    Py_BEGIN_ALLOW_THREADS;
    rc = SQLNumResultCols((SQLHSTMT)stmt_res->hstmt, &nResultCols);
    Py_END_ALLOW_THREADS;

    if (rc == SQL_ERROR || nResultCols == 0)
    {
        _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                        SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
        return -1;
    }

    //if( rc == SQL_SUCCESS_WITH_INFO )
    //{
    //      _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
    //                     SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
    //}

    stmt_res->num_columns = nResultCols;
    stmt_res->column_info = ALLOC_N(IfxPy_result_set_info, nResultCols);
    if (stmt_res->column_info == NULL)
    {
        PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
        return -1;
    }
    memset(stmt_res->column_info, 0, sizeof(IfxPy_result_set_info)*nResultCols);
    /* return a set of attributes for a column */
    for (i = 0; i < nResultCols; i++)
    {
        Py_BEGIN_ALLOW_THREADS;
        rc = SQLDescribeCol((SQLHSTMT)stmt_res->hstmt, (SQLSMALLINT)(i + 1),
            (SQLCHAR *)&tmp_name, BUFSIZ, &name_length,
                            &stmt_res->column_info [i].type,
                            &stmt_res->column_info [i].size,
                            &stmt_res->column_info [i].scale,
                            &stmt_res->column_info [i].nullable);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
        {
            _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                            SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
        }
        if (rc == SQL_ERROR)
        {
            return -1;
        }
        if (name_length <= 0)
        {
            stmt_res->column_info [i].name = (SQLCHAR *)estrdup("");
            if (stmt_res->column_info [i].name == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                return -1;
            }

        }
        else if (name_length >= BUFSIZ)
        {
            /* column name is longer than BUFSIZ */
            stmt_res->column_info [i].name = (SQLCHAR*)ALLOC_N(char, name_length + 1);
            if (stmt_res->column_info [i].name == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                return -1;
            }

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLDescribeCol((SQLHSTMT)stmt_res->hstmt, (SQLSMALLINT)(i + 1),
                                stmt_res->column_info [i].name, name_length,
                                &name_length, &stmt_res->column_info [i].type,
                                &stmt_res->column_info [i].size,
                                &stmt_res->column_info [i].scale,
                                &stmt_res->column_info [i].nullable);
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
            }
            if (rc == SQL_ERROR)
            {
                return -1;
            }

        }
        else
        {
            stmt_res->column_info [i].name = (SQLCHAR*)estrdup((char*)tmp_name);
            if (stmt_res->column_info [i].name == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                return -1;
            }

        }
    }
    return 0;
}

//static int _python_ibn_bind_column_helper(stmt_handle *stmt_res)
//bind columns to data, this must be done once

static int _python_IfxPy_bind_column_helper(stmt_handle *stmt_res)
{
    SQLULEN in_length = 0;
    SQLSMALLINT column_type;
    IfxPy_row_data_type *row_data;
    int i, rc = SQL_SUCCESS;

    stmt_res->row_data = ALLOC_N(IfxPy_row_type, stmt_res->num_columns);
    if (stmt_res->row_data == NULL)
    {
        PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
        return -1;
    }
    memset(stmt_res->row_data, 0, sizeof(IfxPy_row_type)*stmt_res->num_columns);

    for (i = 0; i < stmt_res->num_columns; i++)
    {
        column_type = stmt_res->column_info [i].type;
        row_data = &stmt_res->row_data [i].data;
        switch (column_type)
        {
        case SQL_CHAR:
        case SQL_VARCHAR:
        case SQL_LONGVARCHAR:
	case SQL_INFX_RC_SET:
        case SQL_INFX_RC_MULTISET:
        case SQL_INFX_RC_LIST:
        case SQL_INFX_RC_ROW:
        case SQL_INFX_RC_COLLECTION:
	case SQL_INFX_UDT_FIXED:
	case SQL_INFX_UDT_VARYING:
            if (stmt_res->s_use_wchar == WCHAR_NO)
            {
                in_length = stmt_res->column_info [i].size + 1;
                row_data->str_val = (SQLCHAR *)ALLOC_N(char, in_length);
                if (row_data->str_val == NULL)
                {
                    PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                    return -1;
                }
                rc = SQLBindCol((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)(i + 1),
                                SQL_C_CHAR, row_data->str_val, in_length,
                                (SQLLEN *)(&stmt_res->row_data [i].out_length));
                if (rc == SQL_ERROR)
                {
                    _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                    SQL_HANDLE_STMT, rc, 1, NULL,
                                                    -1, 1);
                }
                break;
            }

        case SQL_WCHAR:
        case SQL_WVARCHAR:
            in_length = stmt_res->column_info [i].size + 1;
            if ( in_length > INT_MAX )
                        /* this is a LO, force SQLFetch truncation to get len */
                        in_length = 0;
            row_data->w_val = (SQLWCHAR *)ALLOC_N(SQLWCHAR, in_length);
            rc = SQLBindCol((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)(i + 1),
                            SQL_C_WCHAR, row_data->w_val, in_length * sizeof(SQLWCHAR),
                            (SQLLEN *)(&stmt_res->row_data [i].out_length));
            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                SQL_HANDLE_STMT, rc, 1, NULL,
                                                -1, 1);
            }
            break;

        case SQL_BINARY:
        case SQL_LONGVARBINARY:
        case SQL_VARBINARY:
            if (stmt_res->s_bin_mode == CONVERT)
            {
                in_length = 2 * (stmt_res->column_info [i].size) + 1;
                row_data->str_val = (SQLCHAR *)ALLOC_N(char, in_length);
                if (row_data->str_val == NULL)
                {
                    PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                    return -1;
                }

                Py_BEGIN_ALLOW_THREADS;
                rc = SQLBindCol((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)(i + 1),
                                SQL_C_CHAR, row_data->str_val, in_length,
                                (SQLLEN *)(&stmt_res->row_data [i].out_length));
                Py_END_ALLOW_THREADS;

                if (rc == SQL_ERROR)
                {
                    _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                    SQL_HANDLE_STMT, rc, 1, NULL,
                                                    -1, 1);
                }
            }
            else
            {
                in_length = stmt_res->column_info [i].size + 1;
                if ( in_length > INT_MAX )
                        /* this is a LO, force SQLFetch truncation to get len */
                        in_length = 0;		    
                row_data->str_val = (SQLCHAR *)ALLOC_N(char, in_length);
                if (row_data->str_val == NULL)
                {
                    PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                    return -1;
                }

                Py_BEGIN_ALLOW_THREADS;
                rc = SQLBindCol((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)(i + 1),
                                SQL_C_DEFAULT, row_data->str_val, in_length,
                                (SQLLEN *)(&stmt_res->row_data [i].out_length));
                Py_END_ALLOW_THREADS;

                if (rc == SQL_ERROR)
                {
                    _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                    SQL_HANDLE_STMT, rc, 1, NULL,
                                                    -1, 1);
                }
            }
            break;

        case SQL_BIGINT:
            in_length = stmt_res->column_info [i].size + 2;
            row_data->str_val = (SQLCHAR *)ALLOC_N(char, in_length);
            if (row_data->str_val == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                return -1;
            }

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindCol((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)(i + 1),
                            SQL_C_CHAR, row_data->str_val, in_length,
                            (SQLLEN *)(&stmt_res->row_data [i].out_length));
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
            }
            break;

        case SQL_TYPE_DATE:
            row_data->date_val = ALLOC(DATE_STRUCT);
            if (row_data->date_val == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                return -1;
            }

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindCol((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)(i + 1),
                            SQL_C_TYPE_DATE, row_data->date_val, sizeof(DATE_STRUCT),
                            (SQLLEN *)(&stmt_res->row_data [i].out_length));
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
            }
            break;

        case SQL_TYPE_TIME:
            row_data->time_val = ALLOC(TIME_STRUCT);
            if (row_data->time_val == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                return -1;
            }

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindCol((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)(i + 1),
                            SQL_C_TYPE_TIME, row_data->time_val, sizeof(TIME_STRUCT),
                            (SQLLEN *)(&stmt_res->row_data [i].out_length));
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
            }
            break;

        case SQL_TYPE_TIMESTAMP:
            row_data->ts_val = ALLOC(TIMESTAMP_STRUCT);
            if (row_data->ts_val == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                return -1;
            }

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindCol((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)(i + 1),
                            SQL_C_TYPE_TIMESTAMP, row_data->time_val, sizeof(TIMESTAMP_STRUCT),
                            (SQLLEN *)(&stmt_res->row_data [i].out_length));
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
                return -1;
            }
            break;

        case SQL_INTERVAL_DAY:
        case SQL_INTERVAL_HOUR:
        case SQL_INTERVAL_MINUTE:
        case SQL_INTERVAL_SECOND:
        case SQL_INTERVAL_DAY_TO_HOUR:
        case SQL_INTERVAL_DAY_TO_MINUTE:
        case SQL_INTERVAL_DAY_TO_SECOND:
        case SQL_INTERVAL_HOUR_TO_MINUTE:
        case SQL_INTERVAL_HOUR_TO_SECOND:
        case SQL_INTERVAL_MINUTE_TO_SECOND:
            row_data->interval_val = ALLOC(SQL_INTERVAL_STRUCT);
            if (row_data->interval_val == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                return -1;
            }

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindCol((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)(i + 1),
                            column_type, row_data->time_val, sizeof(SQL_INTERVAL_STRUCT),
                            (SQLLEN *)(&stmt_res->row_data [i].out_length));
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
                return -1;
            }
            break;

        case SQL_BIT:
        case SQL_SMALLINT:

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindCol((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)(i + 1),
                            SQL_C_DEFAULT, &row_data->s_val,
                            sizeof(row_data->s_val),
                            (SQLLEN *)(&stmt_res->row_data [i].out_length));
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                SQL_HANDLE_STMT, rc, 1, NULL, -1,
                                                1);
            }
            break;

        case SQL_INTEGER:

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindCol((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)(i + 1),
                            SQL_C_DEFAULT, &row_data->i_val,
                            sizeof(row_data->i_val),
                            (SQLLEN *)(&stmt_res->row_data [i].out_length));
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                SQL_HANDLE_STMT, rc, 1, NULL, -1,
                                                1);
            }
            break;

        case SQL_REAL:

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindCol((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)(i + 1),
                            SQL_C_FLOAT, &row_data->r_val,
                            sizeof(row_data->r_val),
                            (SQLLEN *)(&stmt_res->row_data [i].out_length));
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                SQL_HANDLE_STMT, rc, 1, NULL, -1,
                                                1);
            }
            break;

        case SQL_FLOAT:

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindCol((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)(i + 1),
                            SQL_C_DEFAULT, &row_data->f_val,
                            sizeof(row_data->f_val),
                            (SQLLEN *)(&stmt_res->row_data [i].out_length));
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                SQL_HANDLE_STMT, rc, 1, NULL, -1,
                                                1);
            }
            break;

        case SQL_DOUBLE:

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindCol((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)(i + 1),
                            SQL_C_DEFAULT, &row_data->d_val,
                            sizeof(row_data->d_val),
                            (SQLLEN *)(&stmt_res->row_data [i].out_length));
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                SQL_HANDLE_STMT, rc, 1, NULL, -1,
                                                1);
            }
            break;

        case SQL_DECIMAL:
        case SQL_NUMERIC:
            in_length = stmt_res->column_info [i].size +
                stmt_res->column_info [i].scale + 2 + 1;
            row_data->str_val = (SQLCHAR *)ALLOC_N(char, in_length);
            if (row_data->str_val == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                return -1;
            }

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindCol((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)(i + 1),
                            SQL_C_CHAR, row_data->str_val, in_length,
                            (SQLLEN *)(&stmt_res->row_data [i].out_length));
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                                SQL_HANDLE_STMT, rc, 1, NULL, -1,
                                                1);
            }
            break;

        default:
            break;
        }
    }
    return rc;
}

/*    static void _python_IfxPy_clear_stmt_err_cache () */
static void _python_IfxPy_clear_stmt_err_cache(void)
{
    memset(IFX_G(__python_stmt_err_msg), 0, DB_MAX_ERR_MSG_LEN);
    memset(IFX_G(__python_stmt_err_state), 0, SQL_SQLSTATE_SIZE + 1);
}


static PyObject *_python_IfxPy_connect_helper(PyObject *self, PyObject *args, int isPersistent)
{
    PyObject *databaseObj = NULL;
    PyObject *uidObj = NULL;
    PyObject *passwordObj = NULL;
    SQLWCHAR *ConnStrIn = NULL;
    SQLWCHAR *uid = NULL;
    SQLWCHAR *password = NULL;
    PyObject *options = NULL;
    PyObject *literal_replacementObj = NULL;
    PyObject *equal = StringOBJ_FromASCII("=");
    int rc = 0;
    SQLINTEGER conn_alive;
    conn_handle *conn_res = NULL;
    int reused = 0;
    PyObject *hKey = NULL;
    PyObject *entry = NULL;
    char server [2048];
    int isNewBuffer;

    conn_alive = 1;

    if (!PyArg_ParseTuple(args, "OOO|OO", &databaseObj, &uidObj, &passwordObj, &options, &literal_replacementObj))
    {
        return NULL;
    }
    do
    {
        databaseObj = PyUnicode_FromObject(databaseObj);
        uidObj = PyUnicode_FromObject(uidObj);
        passwordObj = PyUnicode_FromObject(passwordObj);

        // Check if we already have a connection for this userID & database combination
        if (isPersistent)
        {
            hKey = PyUnicode_Concat(StringOBJ_FromASCII("__IfxPy_"), uidObj);
            hKey = PyUnicode_Concat(hKey, databaseObj);
            hKey = PyUnicode_Concat(hKey, passwordObj);

            entry = PyDict_GetItem(persistent_list, hKey);

            if (entry != NULL)
            {
                Py_INCREF(entry);
                conn_res = (conn_handle *)entry;


                /* Need to reinitialize connection? */
                rc = SQLGetConnectAttr(conn_res->hdbc, SQL_ATTR_PING_DB,
                    (SQLPOINTER)&conn_alive, 0, NULL);
                if ((rc == SQL_SUCCESS) && conn_alive)
                {
                    _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC,
                                                    rc, 1, NULL, -1, 1);
                    reused = 1;
                } /* else will re-connect since connection is dead */
                reused = 1;
            }
        }
        else
        {
            /* Need to check for max pconnections? */
#if  PY_MAJOR_VERSION >= 3
		if (PyUnicode_GetLength(uidObj)>0)
#else
		if (PyBytes_Size(uidObj)>0)       
#endif
		{
				databaseObj = PyUnicode_Concat(databaseObj, StringOBJ_FromASCII(";UID="));
				databaseObj = PyUnicode_Concat(databaseObj, uidObj);
				databaseObj = PyUnicode_Concat(databaseObj, StringOBJ_FromASCII(";PWD="));
				databaseObj = PyUnicode_Concat(databaseObj, passwordObj);
		}
        }

        if (conn_res == NULL)
        {
            conn_res = PyObject_NEW(conn_handle, &conn_handleType);
            conn_res->henv = 0;
            conn_res->hdbc = 0;
        }

        // We need to set this early, in case we get an error below,
        // so we know how to free the connection 
        conn_res->flag_pconnect = isPersistent;
        /* Allocate ENV handles if not present */
        if (!conn_res->henv)
        {
            rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &(conn_res->henv));
            if (rc != SQL_SUCCESS)
            {
                _python_IfxPy_check_sql_errors(conn_res->henv, SQL_HANDLE_ENV, rc,
                                                1, NULL, -1, 1);
                break;
            }
            rc = SQLSetEnvAttr((SQLHENV)conn_res->henv, SQL_ATTR_ODBC_VERSION,
                (void *)SQL_OV_ODBC3, 0);
        }

        if (!reused)
        {
            /* Alloc CONNECT Handle */
            rc = SQLAllocHandle(SQL_HANDLE_DBC, conn_res->henv, &(conn_res->hdbc));
            if (rc != SQL_SUCCESS)
            {
                _python_IfxPy_check_sql_errors(conn_res->henv, SQL_HANDLE_ENV, rc,
                                                1, NULL, -1, 1);
                break;
            }
        }

        // Set this after the connection handle has been allocated to avoid
        // unnecessary network flows. Initialize the structure to default values
        conn_res->auto_commit = SQL_AUTOCOMMIT_ON;
        rc = SQLSetConnectAttr((SQLHDBC)conn_res->hdbc, SQL_ATTR_AUTOCOMMIT,
            (SQLPOINTER)((SQLLEN)(conn_res->auto_commit)), SQL_NTS);

        conn_res->c_bin_mode = IFX_G(bin_mode);
        conn_res->c_case_mode = CASE_NATURAL;
        conn_res->c_use_wchar = WCHAR_YES;
        conn_res->c_cursor_type = SQL_SCROLL_FORWARD_ONLY;

        conn_res->error_recno_tracker = 1;
        conn_res->errormsg_recno_tracker = 1;

        /* handle not active as of yet */
        conn_res->handle_active = 0;

        /* Set Options */
        if (!NIL_P(options))
        {
            if (!PyDict_Check(options))
            {
                PyErr_SetString(PyExc_Exception, "options Parameter must be of type dictionay");
                return NULL;
            }
            rc = _python_IfxPy_parse_options(options, SQL_HANDLE_DBC, conn_res);
            if (rc != SQL_SUCCESS)
            {
                SQLFreeHandle(SQL_HANDLE_DBC, conn_res->hdbc);
                SQLFreeHandle(SQL_HANDLE_ENV, conn_res->henv);
                break;
            }
        }

        if (!reused)
        {
            /* Connect */
            if (NIL_P(databaseObj))
            {
                PyErr_SetString(PyExc_Exception, "Supplied Parameter is invalid");
                return NULL;
            }

            // Connect to Informix database
            {
                char *cpDTag = ((sizeof(void *) == 8) ?
                    "DRIVER={IBM INFORMIX ODBC DRIVER (64-bit)};" :
                    "DRIVER={IBM INFORMIX ODBC DRIVER};");
    
                PyObject *DriverTag = StringOBJ_FromASCII(cpDTag);                

                databaseObj = PyUnicode_Concat( DriverTag, databaseObj);
                ConnStrIn = getUnicodeDataAsSQLWCHAR(databaseObj, &isNewBuffer);

                rc = SQLDriverConnectW(
                    (SQLHDBC)conn_res->hdbc, // ConnectionHandle
                    NULL,                    // WindowHandle
					ConnStrIn,
                    SQL_NTS,                 // StringLength1 or SQL_NTS
                    NULL,                    // OutConnectionString
                    0,                       // BufferLength - in characters
                    NULL,                    // StringLength2Ptr
                    SQL_DRIVER_NOPROMPT);    // DriverCompletion

                if ( ConnStrIn )
                {
                    PyMem_Del(ConnStrIn);
                    ConnStrIn = NULL;
                }

                // TODO: 
                // Revisit the usage of function returning the return value of 
                // StringOBJ_FromASCII which is PyString_FromString().

                // if you receive a Python object from the Python API, 
                // you can use it within your own C code without INCREFing it.
                
                // if you want to guarantee that the Python object survives past 
                // the end of your own C code, you must INCREF it.
                
                // if you received an object from Python code and it was a new reference, 
                // but you don’t want it to survive past the end of your own C code, 
                // you should DECREF it.

                // Then Python knows that this object is not being used by 
                // anything and can be freed
                Py_XDECREF(DriverTag);
            }

            if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC,
                                                rc, 1, NULL, -1, 1);
            }
            if (rc == SQL_ERROR)
            {
                SQLFreeHandle(SQL_HANDLE_DBC, conn_res->hdbc);
                SQLFreeHandle(SQL_HANDLE_ENV, conn_res->henv);
                break;
            }
            // LO_AUTO
            rc = SQLSetConnectAttr((SQLHDBC)conn_res->hdbc, SQL_INFX_ATTR_LO_AUTOMATIC,	
                                   (SQLPOINTER)SQL_TRUE, SQL_IS_UINTEGER);
		
            // Get the server name
            memset(server, 0, sizeof(server));

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLGetInfo(conn_res->hdbc, SQL_DBMS_NAME, (SQLPOINTER)server,
                            2048, NULL);
            Py_END_ALLOW_THREADS;
        }

        Py_XDECREF(databaseObj);
        Py_XDECREF(uidObj);
        Py_XDECREF(passwordObj);
        conn_res->handle_active = 1;

    } while (0);

    if (hKey != NULL)
    {
        if (!reused && rc == SQL_SUCCESS)
        {
            // If we created a new persistent connection, add it to the persistent_list
            PyDict_SetItem(persistent_list, hKey, (PyObject *)conn_res);
        }
        Py_DECREF(hKey);
    }

    if (isNewBuffer)
    {
        PyMem_Del(uid);
        PyMem_Del(password);
    }

    if (rc != SQL_SUCCESS)
    {
        if (conn_res != NULL && conn_res->handle_active)
        {
            rc = SQLFreeHandle(SQL_HANDLE_DBC, conn_res->hdbc);
            rc = SQLFreeHandle(SQL_HANDLE_ENV, conn_res->henv);
        }
        if (conn_res != NULL)
        {
            PyObject_Del(conn_res);
        }
        return NULL;
    }
    return (PyObject *)conn_res;
}


//This function takes a SQLWCHAR buffer (UCS-2) and returns back a PyUnicode object
// of it that is in the correct current UCS encoding (either UCS2 or UCS4)of
// the current executing python VM
//
// @sqlwcharBytesLen - the length of sqlwcharData in bytes (not characters)

static PyObject* getSQLWCharAsPyUnicodeObject(SQLWCHAR* sqlwcharData, SQLLEN sqlwcharBytesLen)
{
    PyObject *sysmodule = NULL, *maxuni = NULL;
    long maxuniValue;
    PyObject* u;
    sysmodule = PyImport_ImportModule("sys");
    maxuni = PyObject_GetAttrString(sysmodule, "maxunicode");
    maxuniValue = PyInt_AsLong(maxuni);

    if (maxuniValue <= 65536)
    {
        /* this is UCS2 python.. nothing to do really */
#if PY_VERSION_HEX >= 0x03030000
// For Python 3.3 and above
    return PyUnicode_FromWideChar((Py_UNICODE *)sqlwcharData, sqlwcharBytesLen / sizeof(SQLWCHAR));
#else
    // For Python versions below 3.3
    return PyUnicode_FromUnicode((Py_UNICODE *)sqlwcharData, sqlwcharBytesLen / sizeof(SQLWCHAR));
#endif
    }

    if (is_bigendian())
    {
        // *byteorder == -1: little endian
        // *byteorder == 0:  native order
        // *byteorder == 1:  big endian
        int bo = 1;

        if ( sizeof(SQLWCHAR) == 4)
            u = PyUnicode_DecodeUTF32((char *)sqlwcharData, sqlwcharBytesLen, "strict", &bo);
        else
            u = PyUnicode_DecodeUTF16((char *)sqlwcharData, sqlwcharBytesLen, "strict", &bo);
    }
    else
    {
        int bo = -1;

        if ( sizeof(SQLWCHAR) == 4)
            u = PyUnicode_DecodeUTF32((char *)sqlwcharData, sqlwcharBytesLen, "strict", &bo);
        else
            u = PyUnicode_DecodeUTF16((char *)sqlwcharData, sqlwcharBytesLen, "strict", &bo);
    }
    return u;
}

// This function takes value as pyObject and convert it to SQLWCHAR and return it
static SQLWCHAR* getUnicodeDataAsSQLWCHAR(PyObject *pyobj, int *isNewBuffer)
{
	SQLWCHAR* pNewBuffer = NULL;
	Py_ssize_t nCharLen = 0;

	*isNewBuffer = 0;
#if PY_VERSION_HEX >= 0x03030000
// For Python 3.3 and above
    nCharLen = PyUnicode_GetLength(pyobj);
#else
    // For Python versions below 3.3
    nCharLen = PyUnicode_GET_SIZE(pyobj);
#endif

	pNewBuffer = (SQLWCHAR *)ALLOC_N(SQLWCHAR, nCharLen + 1);
	if ( pNewBuffer != NULL)
	{
		Py_ssize_t  NumChar = PyUnicode_AsWideChar( pyobj, pNewBuffer, nCharLen);
		*isNewBuffer = 1;
		pNewBuffer[NumChar] = 0;
	}
	
	return pNewBuffer;
}


// This function takes value as pyObject and convert it to SQLWCHAR and return it
static SQLWCHAR* xgetUnicodeDataAsSQLWCHAR(PyObject *pyobj, int *isNewBuffer)
{
    PyObject *sysmodule = NULL, *maxuni = NULL;
    long maxuniValue;
    PyObject *pyUTFobj;
    SQLWCHAR* pNewBuffer = NULL;
#if PY_VERSION_HEX >= 0x03030000
// For Python 3.3 and above
    Py_ssize_t nCharLen = PyUnicode_GetSize(pyobj);
#else
    // For Python versions below 3.3
    Py_ssize_t nCharLen = PyUnicode_GET_SIZE(pyobj);
#endif


    sysmodule = PyImport_ImportModule("sys");
    maxuni = PyObject_GetAttrString(sysmodule, "maxunicode");
    maxuniValue = PyInt_AsLong(maxuni);

    if (maxuniValue <= 65536)
    {
        *isNewBuffer = 0;
        return (SQLWCHAR*)PyUnicode_AS_UNICODE(pyobj);
    }

    *isNewBuffer = 1;
    pNewBuffer = (SQLWCHAR *)ALLOC_N(SQLWCHAR, nCharLen + 1);
    memset(pNewBuffer, 0, sizeof(SQLWCHAR) * (nCharLen + 1));
    if (is_bigendian())
    {
        pyUTFobj = PyCodec_Encode(pyobj, "utf-16-be", "strict");
    }
    else
    {
        pyUTFobj = PyCodec_Encode(pyobj, "utf-16-le", "strict");
    }
    memcpy(pNewBuffer, PyBytes_AsString(pyUTFobj), sizeof(SQLWCHAR) * (nCharLen));
    Py_DECREF(pyUTFobj);
    return pNewBuffer;

}



/* static void _python_IfxPy_clear_conn_err_cache () */
static void _python_IfxPy_clear_conn_err_cache(void)
{
    /* Clear out the cached conn messages */
    memset(IFX_G(__python_conn_err_msg), 0, DB_MAX_ERR_MSG_LEN);
    memset(IFX_G(__python_conn_err_state), 0, SQL_SQLSTATE_SIZE + 1);
}

/*!#
* IfxPy.connect
* IfxPy.autocommit
* IfxPy.bind_param
* IfxPy.close
* IfxPy.column_privileges
* IfxPy.columns
* IfxPy.foreign_keys
* IfxPy.primary_keys
* IfxPy.procedure_columns
* IfxPy.procedures
* IfxPy.special_columns
* IfxPy.statistics
* IfxPy.table_privileges
* IfxPy.tables
* IfxPy.commit
* IfxPy.exec
* IfxPy.free_result
* IfxPy.prepare
* IfxPy.execute
* IfxPy.conn_errormsg
* IfxPy.stmt_errormsg
* IfxPy.conn_error
* IfxPy.stmt_error
* IfxPy.next_result
* IfxPy.num_fields
* IfxPy.num_rows
* IfxPy.get_num_result
* IfxPy.field_name
* IfxPy.field_display_size
* IfxPy.field_num
* IfxPy.field_precision
* IfxPy.field_scale
* IfxPy.field_type
* IfxPy.field_width
* IfxPy.cursor_type
* IfxPy.rollback
* IfxPy.free_stmt
* IfxPy.result
* IfxPy.fetch_row
* IfxPy.fetch_assoc
* IfxPy.fetch_array
* IfxPy.fetch_both
* IfxPy.set_option
* IfxPy.server_info
* IfxPy.client_info
* IfxPy.active
* IfxPy.get_option
*/



/*!# IfxPy.connect
*
* ===Description
*
*  --    Returns a connection to a database
* IFXConnection IfxPy.connect (dsn=<..>, user=<..>, password=<..>,
*                                  host=<..>, database=<..>, options=<..>)
*
* Creates a new connection to an Informix Database,
* or Apache Derby database.
*
* ===Parameters
*
* ====dsn
*
* For an uncataloged connection to a database, database represents a complete
* connection string in the following format:
* DRIVER=DATABASE=database;HOST=hostname;PORT=port;
* PROTOCOL=TCPIP;UID=username;PWD=password;
*      where the parameters represent the following values:
*        hostname
*            The hostname or IP address of the database server.
*        port
*            The TCP/IP port on which the database is listening for requests.
*        username
*            The username with which you are connecting to the database.
*        password
*            The password with which you are connecting to the database.
*
* ====user
*
* The username with which you are connecting to the database.
* This is optional if username is specified in the "dsn" string
*
* ====password
*
* The password with which you are connecting to the database.
* This is optional if password is specified in the "dsn" string
*
* ====host
*
* The hostname or IP address of the database server.
* This is optional if hostname is specified in the "dsn" string
*
* ====database
*
* For a cataloged connection to a database, database represents the database
* alias in the DB2 client catalog.
* This is optional if database is specified in the "dsn" string
*
* ====options
*
*      An dictionary of connection options that affect the behavior of the
*      connection,
*      where valid array keys include:
*        SQL_ATTR_AUTOCOMMIT
*            Passing the SQL_AUTOCOMMIT_ON value turns autocommit on for this
*            connection handle.
*            Passing the SQL_AUTOCOMMIT_OFF value turns autocommit off for this
*            connection handle.
*        ATTR_CASE
*            Passing the CASE_NATURAL value specifies that column names are
*            returned in natural case.
*            Passing the CASE_LOWER value specifies that column names are
*            returned in lower case.
*            Passing the CASE_UPPER value specifies that column names are
*            returned in upper case.
*        SQL_ATTR_CURSOR_TYPE
*            Passing the SQL_SCROLL_FORWARD_ONLY value specifies a forward-only
*            cursor for a statement resource.
*            This is the default cursor type and is supported on all database
*            servers.
*            Passing the SQL_CURSOR_KEYSET_DRIVEN value specifies a scrollable
*            cursor for a statement resource.
*            This mode enables random access to rows in a result set, but
*            currently is supported only by IBM DB2 Universal Database.
*
* ===Return Values
*
*
* Returns a IFXConnection connection object if the connection attempt is
* successful.
* If the connection attempt fails, IfxPy.connect() returns None.
*
*/
static PyObject *IfxPy_connect(PyObject *self, PyObject *args)
{
    _python_IfxPy_clear_conn_err_cache();
    return _python_IfxPy_connect_helper(self, args, 0);
}


// static void _python_clear_local_var(PyObject *dbNameObj, SQLWCHAR *dbName, PyObject *codesetObj, SQLWCHAR *codesetObj, PyObject *modeObj, SQLWCHAR *mode, int isNewBuffer)
static void _python_clear_local_var(PyObject *dbNameObj, SQLWCHAR *dbName, PyObject *codesetObj, SQLWCHAR *codeset, PyObject *modeObj, SQLWCHAR *mode, int isNewBuffer)
{
    if (!NIL_P(dbNameObj))
    {
        Py_XDECREF(dbNameObj);
        if (isNewBuffer)
        {
            PyMem_Del(dbName);
        }
    }

    if (!NIL_P(codesetObj))
    {
        Py_XDECREF(codesetObj);
        if (isNewBuffer)
        {
            PyMem_Del(codeset);
        }
    }

    if (!NIL_P(modeObj))
    {
        Py_XDECREF(modeObj);
        if (isNewBuffer)
        {
            PyMem_Del(mode);
        }
    }
}



/*!# IfxPy.autocommit
*
* ===Description
*
* mixed IfxPy.autocommit ( resource connection [, bool value] )
*
* Returns or sets the AUTOCOMMIT behavior of the specified connection resource.
*
* ===Parameters
*
* ====connection
*    A valid database connection resource variable as returned from connect()
*
* ====value
*    One of the following constants:
*    SQL_AUTOCOMMIT_OFF
*          Turns AUTOCOMMIT off.
*    SQL_AUTOCOMMIT_ON
*          Turns AUTOCOMMIT on.
*
* ===Return Values
*
* When IfxPy.autocommit() receives only the connection parameter, it returns
* the current state of AUTOCOMMIT for the requested connection as an integer
* value. A value of 0 indicates that AUTOCOMMIT is off, while a value of 1
* indicates that AUTOCOMMIT is on.
*
* When IfxPy.autocommit() receives both the connection parameter and
* autocommit parameter, it attempts to set the AUTOCOMMIT state of the
* requested connection to the corresponding state.
*
* Returns TRUE on success or FALSE on failure.
*/
static PyObject *IfxPy_autocommit(PyObject *self, PyObject *args)
{
    PyObject *py_autocommit = NULL;
    PyObject *py_conn_res = NULL;
    conn_handle *conn_res = NULL;
    int rc;
    SQLINTEGER autocommit = -1;

    if (!PyArg_ParseTuple(args, "O|O", &py_conn_res, &py_autocommit))
    {
        return NULL;
    }

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }
        if (!NIL_P(py_autocommit))
        {
            if (PyInt_Check(py_autocommit))
            {
                autocommit = (SQLINTEGER)PyInt_AsLong(py_autocommit);
            }
            else
            {
                PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
                return NULL;
            }
        }
        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            return NULL;
        }

        /* If value in handle is different from value passed in */
        if (PyTuple_Size(args) == 2)
        {
            if (autocommit != (conn_res->auto_commit))
            {
                rc = SQLSetConnectAttr((SQLHDBC)conn_res->hdbc, SQL_ATTR_AUTOCOMMIT,
                    (SQLPOINTER)((SQLULEN)(autocommit == 0 ? SQL_AUTOCOMMIT_OFF : SQL_AUTOCOMMIT_ON)), SQL_IS_INTEGER);
                if (rc == SQL_ERROR)
                {
                    _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC,
                                                    rc, 1, NULL, -1, 1);
                }
                conn_res->auto_commit = autocommit;
            }
            Py_INCREF(Py_True);
            return Py_True;
        }
        else
        {
            return PyInt_FromLong(conn_res->auto_commit);
        }
    }
    return NULL;
}

static void _python_IfxPy_add_param_cache(
    stmt_handle *stmt_res,
    int param_no,
    PyObject *var_pyvalue,
    int param_type,
    int size,
    SQLSMALLINT data_type,
    SQLULEN precision,
    SQLSMALLINT scale,
    SQLSMALLINT nullable)
{
    param_node *tmp_curr = NULL, *prev = stmt_res->head_cache_list, *curr = stmt_res->head_cache_list;

    while ((curr != NULL) && (curr->param_num < param_no))
    {
        prev = curr;
        curr = curr->next;
    }

    if (curr == NULL || curr->param_num != param_no)
    {
        /* Allocate memory and make new node to be added */
        tmp_curr = ALLOC(param_node);
        memset(tmp_curr, 0, sizeof(param_node));

        /* assign values */
        tmp_curr->data_type = data_type;
        tmp_curr->param_size = precision;
        tmp_curr->nullable = nullable;
        tmp_curr->scale = scale;
        tmp_curr->param_num = param_no;
        tmp_curr->file_options = SQL_FILE_READ;
        tmp_curr->param_type = param_type;
        tmp_curr->size = size;

        /* Set this flag in stmt_res if a FILE INPUT is present */
        if (param_type == PARAM_FILE)
        {
            stmt_res->file_param = 1;
        }

        if (var_pyvalue != NULL)
        {
            Py_INCREF(var_pyvalue);
            tmp_curr->var_pyvalue = var_pyvalue;
        }

        /* link pointers for the list */
        if (prev == NULL)
        {
            stmt_res->head_cache_list = tmp_curr;
        }
        else
        {
            prev->next = tmp_curr;
        }
        tmp_curr->next = curr;

        /* Increment num params added */
        stmt_res->num_params++;
    }
    else
    {
        /* Both the nodes are for the same param no */
        /* Replace Information */
        curr->data_type = data_type;
        curr->param_size = precision;
        curr->nullable = nullable;
        curr->scale = scale;
        curr->param_num = param_no;
        curr->file_options = SQL_FILE_READ;
        curr->param_type = param_type;
        curr->size = size;

        /* Set this flag in stmt_res if a FILE INPUT is present */
        if (param_type == PARAM_FILE)
        {
            stmt_res->file_param = 1;
        }

        if (var_pyvalue != NULL)
        {
            Py_DECREF(curr->var_pyvalue);
            Py_INCREF(var_pyvalue);
            curr->var_pyvalue = var_pyvalue;
        }

    }
}


// static PyObject *_python_IfxPy_bind_param_helper(int argc, stmt_handle *stmt_res, SQLUSMALLINT param_no, PyObject *var_pyvalue, long param_type,
//                      long data_type, long precision, long scale, long size)

static PyObject *_python_IfxPy_bind_param_helper(int argc, stmt_handle *stmt_res, SQLUSMALLINT param_no, PyObject *var_pyvalue, long param_type, long data_type, long precision, long scale, long size)
{
    SQLSMALLINT sql_data_type = 0;
    SQLULEN     sql_precision = 0;
    SQLSMALLINT sql_scale = 0;
    SQLSMALLINT sql_nullable = SQL_NO_NULLS;
    char error [DB_MAX_ERR_MSG_LEN];
    int rc = 0;

    /* Check for Param options */
    switch (argc)
    {
        /* if argc == 3, then the default value for param_type will be used */
    case 3:
        param_type = SQL_PARAM_INPUT;

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLDescribeParam((SQLHSTMT)stmt_res->hstmt, (SQLUSMALLINT)param_no, &sql_data_type,
                              &sql_precision, &sql_scale, &sql_nullable);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_SUCCESS_WITH_INFO || rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt,
                                            SQL_HANDLE_STMT, rc, 1,
                                            NULL, -1, 1);
        }
        if (rc == SQL_ERROR)
        {
            sprintf(error, "Describe Param Failed: %s",
                    IFX_G(__python_stmt_err_msg));
            PyErr_SetString(PyExc_Exception, error);
            return NULL;
        }
        /* Add to cache */
        _python_IfxPy_add_param_cache(stmt_res, param_no, var_pyvalue,
                                       param_type, size,
                                       sql_data_type, sql_precision,
                                       sql_scale, sql_nullable);
        break;

    case 4:
        Py_BEGIN_ALLOW_THREADS;
        rc = SQLDescribeParam((SQLHSTMT)stmt_res->hstmt,
            (SQLUSMALLINT)param_no, &sql_data_type,
                              &sql_precision, &sql_scale, &sql_nullable);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_SUCCESS_WITH_INFO || rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt,
                                            SQL_HANDLE_STMT, rc, 1,
                                            NULL, -1, 1);
        }
        if (rc == SQL_ERROR)
        {
            sprintf(error, "Describe Param Failed: %s",
                    IFX_G(__python_stmt_err_msg));
            PyErr_SetString(PyExc_Exception, error);
            return NULL;
        }
        /* Add to cache */
        _python_IfxPy_add_param_cache(stmt_res, param_no, var_pyvalue,
                                       param_type, size,
                                       sql_data_type, sql_precision,
                                       sql_scale, sql_nullable);
        break;

    case 5:
        Py_BEGIN_ALLOW_THREADS;
        rc = SQLDescribeParam((SQLHSTMT)stmt_res->hstmt,
            (SQLUSMALLINT)param_no, &sql_data_type,
                              &sql_precision, &sql_scale, &sql_nullable);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_SUCCESS_WITH_INFO || rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt,
                                            SQL_HANDLE_STMT, rc, 1,
                                            NULL, -1, 1);
        }
        if (rc == SQL_ERROR)
        {
            sprintf(error, "Describe Param Failed: %s",
                    IFX_G(__python_stmt_err_msg));
            PyErr_SetString(PyExc_Exception, error);
            return NULL;
        }
        sql_data_type = (SQLSMALLINT)data_type;
        /* Add to cache */
        _python_IfxPy_add_param_cache(stmt_res, param_no, var_pyvalue,
                                       param_type, size,
                                       sql_data_type, sql_precision,
                                       sql_scale, sql_nullable);
        break;

    case 6:
        Py_BEGIN_ALLOW_THREADS;
        rc = SQLDescribeParam((SQLHSTMT)stmt_res->hstmt,
            (SQLUSMALLINT)param_no, &sql_data_type,
                              &sql_precision, &sql_scale, &sql_nullable);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_SUCCESS_WITH_INFO || rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt,
                                            SQL_HANDLE_STMT, rc, 1,
                                            NULL, -1, 1);
        }
        if (rc == SQL_ERROR)
        {
            sprintf(error, "Describe Param Failed: %s",
                    IFX_G(__python_stmt_err_msg));
            PyErr_SetString(PyExc_Exception, error);
            return NULL;
        }
        sql_data_type = (SQLSMALLINT)data_type;
        sql_precision = (SQLUINTEGER)precision;
        /* Add to cache */
        _python_IfxPy_add_param_cache(stmt_res, param_no, var_pyvalue,
                                       param_type, size,
                                       sql_data_type, sql_precision,
                                       sql_scale, sql_nullable);
        break;

    case 7:
    case 8:
        /* Cache param data passed
        * I am using a linked list of nodes here because I don't know
        * before hand how many params are being passed in/bound.
        * To determine this, a call to SQLNumParams is necessary.
        * This is take away any advantages an array would have over
        * linked list access
        * Data is being copied over to the correct types for subsequent
        * CLI call because this might cause problems on other platforms
        * such as AIX
        */
        sql_data_type = (SQLSMALLINT)data_type;
        sql_precision = (SQLUINTEGER)precision;
        sql_scale = (SQLSMALLINT)scale;
        _python_IfxPy_add_param_cache(stmt_res, param_no, var_pyvalue,
                                       param_type, size,
                                       sql_data_type, sql_precision,
                                       sql_scale, sql_nullable);
        break;

    default:
        /* WRONG_PARAM_COUNT; */
        return NULL;
    }
    /* end Switch */

    /* We bind data with DB2 CLI in IfxPy.execute() */
    /* This will save network flow if we need to override params in it */

    Py_INCREF(Py_True);
    return Py_True;
}

/*!# IfxPy.bind_param
*
* ===Description
* Py_True/Py_None IfxPy.bind_param (resource stmt, int parameter-number,
*                                    string variable [, int parameter-type
*                                    [, int data-type [, int precision
*                                    [, int scale [, int size[]]]]]] )
*
* Binds a Python variable to an SQL statement parameter in a IFXStatement
* resource returned by IfxPy.prepare().
* This function gives you more control over the parameter type, data type,
* precision, and scale for the parameter than simply passing the variable as
* part of the optional input array to IfxPy.execute().
*
* ===Parameters
*
* ====stmt
*
*    A prepared statement returned from IfxPy.prepare().
*
* ====parameter-number
*
*    Specifies the 1-indexed position of the parameter in the prepared
* statement.
*
* ====variable
*
*    A Python variable to bind to the parameter specified by parameter-number.
*
* ====parameter-type
*
*    A constant specifying whether the Python variable should be bound to the
* SQL parameter as an input parameter (SQL_PARAM_INPUT), an output parameter
* (SQL_PARAM_OUTPUT), or as a parameter that accepts input and returns output
* (SQL_PARAM_INPUT_OUTPUT). To avoid memory overhead, you can also specify
* PARAM_FILE to bind the Python variable to the name of a file that contains
* large object (BLOB, CLOB, or DBCLOB) data.
*
* ====data-type
*
*    A constant specifying the SQL data type that the Python variable should be
* bound as: one of SQL_BINARY, DB2_CHAR, DB2_DOUBLE, or DB2_LONG .
*
* ====precision
*
*    Specifies the precision that the variable should be bound to the database. *
* ====scale
*
*      Specifies the scale that the variable should be bound to the database.
*
* ====size
*
*      Specifies the size that should be retreived from an INOUT/OUT parameter.
*
* ===Return Values
*
*    Returns Py_True on success or NULL on failure.
*/
static PyObject *IfxPy_bind_param(PyObject *self, PyObject *args)
{
    PyObject *var_pyvalue = NULL;
    PyObject *py_param_type = NULL;
    PyObject *py_data_type = NULL;
    PyObject *py_precision = NULL;
    PyObject *py_scale = NULL;
    PyObject *py_size = NULL;
    PyObject *py_param_no = NULL;
    PyObject *py_stmt_res = NULL;

    long param_type = SQL_PARAM_INPUT;
    /* LONG types used for data being passed in */
    SQLUSMALLINT param_no = 0;
    long data_type = 0;
    long precision = 0;
    long scale = 0;
    long size = 0;
    stmt_handle *stmt_res;

    if (!PyArg_ParseTuple(args, "OOO|OOOOO", &py_stmt_res, &py_param_no,
        &var_pyvalue, &py_param_type,
        &py_data_type, &py_precision,
        &py_scale, &py_size))
    {

        return NULL;
    }

    if (!NIL_P(py_param_no))
    {
        if (PyInt_Check(py_param_no))
        {
            param_no = (SQLUSMALLINT)PyInt_AsLong(py_param_no);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
            return NULL;
        }
    }
    if (py_param_type != NULL && py_param_type != Py_None &&
        TYPE(py_param_type) == PYTHON_FIXNUM)
    {
        param_type = PyInt_AS_LONG(py_param_type);
    }

    if (py_data_type != NULL && py_data_type != Py_None &&
        TYPE(py_data_type) == PYTHON_FIXNUM)
    {
        data_type = PyInt_AS_LONG(py_data_type);
    }

    if (py_precision != NULL && py_precision != Py_None &&
        TYPE(py_precision) == PYTHON_FIXNUM)
    {
        precision = PyInt_AS_LONG(py_precision);
    }

    if (py_scale != NULL && py_scale != Py_None &&
        TYPE(py_scale) == PYTHON_FIXNUM)
    {
        scale = PyInt_AS_LONG(py_scale);
    }

    if (py_size != NULL && py_size != Py_None &&
        TYPE(py_size) == PYTHON_FIXNUM)
    {
        size = PyInt_AS_LONG(py_size);
    }
    
     if (!NIL_P(py_stmt_res))
    {
        if (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
            return NULL;
        }
        else
        {
            stmt_res = (stmt_handle *)py_stmt_res;
        }
        return _python_IfxPy_bind_param_helper((int)PyTuple_Size(args),
                                                stmt_res, param_no, var_pyvalue, param_type, data_type, precision, scale, size);
    }
    else
    {
        PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
        return NULL;
    }
}


/*!# IfxPy.close
*
* ===Description
*
* bool IfxPy.close ( resource connection )
*
* This function closes a IDS client connection created with IfxPy.connect()
* and returns the corresponding resources to the database server.
*
*
* ===Parameters
*
* ====connection
*    Specifies an active IDS client connection.
*
* ===Return Values
* Returns TRUE on success or FALSE on failure.
*/
static PyObject *IfxPy_close(PyObject *self, PyObject *args)
{
    PyObject *py_conn_res = NULL;
    conn_handle *conn_res = NULL;
    int rc;
    storeSessionID *tempNode = NULL;
	storeSessionID *delNode = NULL;

    if (!PyArg_ParseTuple(args, "O", &py_conn_res))
        return NULL;

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }

        /* Check to see if it's a persistent connection;
        * if so, just return true
        */

        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            return NULL;
        }

        if (conn_res->handle_active && !conn_res->flag_pconnect)
        {
            // Disconnect from DB. If stmt is allocated, it is freed automatically
            if (conn_res->auto_commit == 0)
            {
                rc = SQLEndTran(SQL_HANDLE_DBC, (SQLHDBC)conn_res->hdbc,
                                SQL_ROLLBACK);
                // if (rc == SQL_ERROR)
                // {
                //     _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC,
                //                                     rc, 1, NULL, -1, 1);
                //     return NULL;
                // }
            }
            rc = SQLDisconnect((SQLHDBC)conn_res->hdbc);
            if (rc == SQL_SUCCESS_WITH_INFO || rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            if (rc == SQL_ERROR)
            {
                return NULL;
            }

            tempNode = rootNode;
			while (tempNode)
			{
				delNode = tempNode;
				tempNode = tempNode->next;
				free(delNode);
				delNode = NULL;
			}

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLFreeHandle(SQL_HANDLE_DBC, conn_res->hdbc);
            Py_END_ALLOW_THREADS;

            if (rc == SQL_SUCCESS_WITH_INFO || rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }

            if (rc == SQL_ERROR)
            {

                rc = SQLFreeHandle(SQL_HANDLE_ENV, conn_res->henv);
                return NULL;
            }

            rc = SQLFreeHandle(SQL_HANDLE_ENV, conn_res->henv);
            if (rc == SQL_SUCCESS_WITH_INFO || rc == SQL_ERROR)
            {
                _python_IfxPy_check_sql_errors(conn_res->henv,
                                                SQL_HANDLE_ENV, rc, 1,
                                                NULL, -1, 1);
            }

            if (rc == SQL_ERROR)
            {
                return NULL;
            }

            conn_res->handle_active = 0;
            Py_INCREF(Py_True);
            return Py_True;
        }
        else if (conn_res->flag_pconnect)
        {
            /* Do we need to call FreeStmt or something to close cursors? */
            Py_INCREF(Py_True);
            return Py_True;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        return NULL;
    }
}

/*!# IfxPy.column_privileges
*
* ===Description
* resource IfxPy.column_privileges ( resource connection [, string qualifier
* [, string schema [, string table-name [, string column-name]]]] )
*
* Returns a result set listing the columns and associated privileges for a
* table.
*
* ===Parameters
*
* ====connection
*        A valid connection to an IDS database.
*
* ====qualifier
*        A qualifier for DB2 databases running on OS/390 or z/OS servers. For
* other databases, pass NULL or an empty string.
*
* ====schema
*        The schema which contains the tables. To match all schemas, pass NULL
* or an empty string.
*
* ====table-name
*        The name of the table or view. To match all tables in the database,
* pass NULL or an empty string.
*
* ====column-name
*        The name of the column. To match all columns in the table, pass NULL
* or an empty string.
*
* ===Return Values
* Returns a statement resource with a result set containing rows describing
* the column privileges for columns matching the specified parameters. The rows
* are composed of the following columns:
*
* TABLE_CAT:: Name of the catalog. The value is NULL if this table does not
* have catalogs.
* TABLE_SCHEM:: Name of the schema.
* TABLE_NAME:: Name of the table or view.
* COLUMN_NAME:: Name of the column.
* GRANTOR:: Authorization ID of the user who granted the privilege.
* GRANTEE:: Authorization ID of the user to whom the privilege was granted.
* PRIVILEGE:: The privilege for the column.
* IS_GRANTABLE:: Whether the GRANTEE is permitted to grant this privilege to
* other users.
*/
static PyObject *IfxPy_column_privileges(PyObject *self, PyObject *args)
{
    SQLWCHAR *qualifier = NULL;
    SQLWCHAR *owner = NULL;
    SQLWCHAR *table_name = NULL;
    SQLWCHAR *column_name = NULL;
    PyObject *py_qualifier = NULL;
    PyObject *py_owner = NULL;
    PyObject *py_table_name = NULL;
    PyObject *py_column_name = NULL;
    PyObject *py_conn_res = NULL;
    conn_handle *conn_res;
    stmt_handle *stmt_res;
    int rc;
    int isNewBuffer;

    if (!PyArg_ParseTuple(args, "O|OOOO", &py_conn_res, &py_qualifier, &py_owner,
        &py_table_name, &py_column_name))
        return NULL;

    if (py_qualifier != NULL && py_qualifier != Py_None)
    {
        if (PyString_Check(py_qualifier) || PyUnicode_Check(py_qualifier))
        {
            py_qualifier = PyUnicode_FromObject(py_qualifier);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "qualifier must be a string or unicode");
            return NULL;
        }
    }

    if (py_owner != NULL && py_owner != Py_None)
    {
        if (PyString_Check(py_owner) || PyUnicode_Check(py_owner))
        {
            py_owner = PyUnicode_FromObject(py_owner);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "owner must be a string or unicode");
            Py_XDECREF(py_qualifier);
            return NULL;
        }
    }

    if (py_table_name != NULL && py_table_name != Py_None)
    {
        if (PyString_Check(py_table_name) || PyUnicode_Check(py_table_name))
        {
            py_table_name = PyUnicode_FromObject(py_table_name);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "table_name must be a string or unicode");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            return NULL;
        }
    }

    if (py_column_name != NULL && py_column_name != Py_None)
    {
        if (PyString_Check(py_column_name) || PyUnicode_Check(py_table_name))
        {
            py_column_name = PyUnicode_FromObject(py_column_name);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "column_name must be a string");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            return NULL;
        }
    }

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }

        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            Py_XDECREF(py_column_name);
            return NULL;
        }

        stmt_res = _IfxPy_new_stmt_struct(conn_res);
        rc = SQLAllocHandle(SQL_HANDLE_STMT, conn_res->hdbc, &(stmt_res->hstmt));
        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            Py_XDECREF(py_column_name);

            Py_RETURN_FALSE;
        }
        if (py_qualifier && py_qualifier != Py_None)
            qualifier = getUnicodeDataAsSQLWCHAR(py_qualifier, &isNewBuffer);
        if (py_owner &&  py_owner != Py_None)
            owner = getUnicodeDataAsSQLWCHAR(py_owner, &isNewBuffer);
        if (py_table_name && py_table_name != Py_None)
            table_name = getUnicodeDataAsSQLWCHAR(py_table_name, &isNewBuffer);

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLColumnPrivilegesW((SQLHSTMT)stmt_res->hstmt, qualifier, SQL_NTS,
                                  owner, SQL_NTS, table_name, SQL_NTS, column_name,
                                  SQL_NTS);
        Py_END_ALLOW_THREADS;

        if (isNewBuffer)
        {
            if (qualifier) PyMem_Del(qualifier);
            if (owner) PyMem_Del(owner);
            if (table_name) PyMem_Del(table_name);
        }

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                            SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            Py_XDECREF(py_column_name);

            Py_RETURN_FALSE;
        }
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_table_name);
        Py_XDECREF(py_column_name);

        return (PyObject *)stmt_res;
    }
    else
    {
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_table_name);
        Py_XDECREF(py_column_name);

        Py_RETURN_FALSE;
    }
}

/*!# IfxPy.columns
* ===Description
* resource IfxPy.columns ( resource connection [, string qualifier
* [, string schema [, string table-name [, string column-name]]]] )
*
* Returns a result set listing the columns and associated metadata for a table.
*
* ===Parameters
* ====connection
*        A valid connection to an IDS database.
*
* ====qualifier
*        A qualifier for DB2 databases running on OS/390 or z/OS servers. For
* other databases, pass NULL or an empty string.
*
* ====schema
*        The schema which contains the tables. To match all schemas, pass '%'.
*
* ====table-name
*        The name of the table or view. To match all tables in the database,
* pass NULL or an empty string.
*
* ====column-name
*        The name of the column. To match all columns in the table, pass NULL or
* an empty string.
*
* ===Return Values
* Returns a statement resource with a result set containing rows describing the
* columns matching the specified parameters.
* The rows are composed of the following columns:
*
* TABLE_CAT:: Name of the catalog. The value is NULL if this table does not
* have catalogs.
* TABLE_SCHEM:: Name of the schema.
* TABLE_NAME:: Name of the table or view.
* COLUMN_NAME:: Name of the column.
* DATA_TYPE:: The SQL data type for the column represented as an integer value.
* TYPE_NAME:: A string representing the data type for the column.
* COLUMN_SIZE:: An integer value representing the size of the column.
* BUFFER_LENGTH:: Maximum number of bytes necessary to store data from this
* column.
* DECIMAL_DIGITS:: The scale of the column, or NULL where scale is not
* applicable.
* NUM_PREC_RADIX:: An integer value of either 10 (representing an exact numeric
* data type), 2 (representing an approximate numeric data type), or NULL
* (representing a data type for which radix is not applicable).
* NULLABLE:: An integer value representing whether the column is nullable or
* not.
* REMARKS:: Description of the column.
* COLUMN_DEF:: Default value for the column.
* SQL_DATA_TYPE:: An integer value representing the size of the column.
* SQL_DATETIME_SUB:: Returns an integer value representing a datetime subtype
* code, or NULL for SQL data types to which this does not apply.
* CHAR_OCTET_LENGTH::    Maximum length in octets for a character data type
* column, which matches COLUMN_SIZE for single-byte character set data, or
* NULL for non-character data types.
* ORDINAL_POSITION:: The 1-indexed position of the column in the table.
* IS_NULLABLE:: A string value where 'YES' means that the column is nullable
* and 'NO' means that the column is not nullable.
*/
static PyObject *IfxPy_columns(PyObject *self, PyObject *args)
{
    SQLWCHAR *qualifier = NULL;
    SQLWCHAR *owner = NULL;
    SQLWCHAR *table_name = NULL;
    SQLWCHAR *column_name = NULL;
    PyObject *py_qualifier = NULL;
    PyObject *py_owner = NULL;
    PyObject *py_table_name = NULL;
    PyObject *py_column_name = NULL;
    PyObject *py_conn_res = NULL;
    conn_handle *conn_res;
    stmt_handle *stmt_res;
    int rc;
    int isNewBuffer;

    if (!PyArg_ParseTuple(args, "O|OOOO", &py_conn_res, &py_qualifier, &py_owner,
        &py_table_name, &py_column_name))
        return NULL;

    if (py_qualifier != NULL && py_qualifier != Py_None)
    {
        if (PyString_Check(py_qualifier) || PyUnicode_Check(py_qualifier))
        {
            py_qualifier = PyUnicode_FromObject(py_qualifier);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "qualifier must be a string or unicode");
            return NULL;
        }
    }

    if (py_owner != NULL && py_owner != Py_None)
    {
        if (PyString_Check(py_owner) || PyUnicode_Check(py_owner))
        {
            py_owner = PyUnicode_FromObject(py_owner);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "owner must be a string or unicode");
            Py_XDECREF(py_qualifier);
            return NULL;
        }
    }

    if (py_table_name != NULL && py_table_name != Py_None)
    {
        if (PyString_Check(py_table_name) || PyUnicode_Check(py_table_name))
        {
            py_table_name = PyUnicode_FromObject(py_table_name);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "table_name must be a string or unicode");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            return NULL;
        }
    }

    if (py_column_name != NULL && py_column_name != Py_None)
    {
        if (PyString_Check(py_column_name) || PyUnicode_Check(py_table_name))
        {
            py_column_name = PyUnicode_FromObject(py_column_name);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "column_name must be a string");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            return NULL;
        }
    }

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }

        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            Py_XDECREF(py_column_name);
            return NULL;
        }

        stmt_res = _IfxPy_new_stmt_struct(conn_res);

        rc = SQLAllocHandle(SQL_HANDLE_STMT, conn_res->hdbc, &(stmt_res->hstmt));
        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            Py_XDECREF(py_column_name);

            Py_RETURN_FALSE;
        }

        if (py_qualifier && py_qualifier != Py_None)
            qualifier = getUnicodeDataAsSQLWCHAR(py_qualifier, &isNewBuffer);
        if (py_owner &&  py_owner != Py_None)
            owner = getUnicodeDataAsSQLWCHAR(py_owner, &isNewBuffer);
        if (py_table_name && py_table_name != Py_None)
            table_name = getUnicodeDataAsSQLWCHAR(py_table_name, &isNewBuffer);
        if (py_column_name && py_column_name != Py_None)
            column_name = getUnicodeDataAsSQLWCHAR(py_column_name, &isNewBuffer);

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLColumnsW((SQLHSTMT)stmt_res->hstmt, qualifier, SQL_NTS,
                         owner, SQL_NTS, table_name, SQL_NTS, column_name, SQL_NTS);
        Py_END_ALLOW_THREADS;

        if (isNewBuffer)
        {
            if (qualifier) PyMem_Del(qualifier);
            if (owner) PyMem_Del(owner);
            if (table_name) PyMem_Del(table_name);
            if (column_name) PyMem_Del(column_name);
        }

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                            SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            Py_XDECREF(py_column_name);

            Py_RETURN_FALSE;
        }
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_table_name);
        Py_XDECREF(py_column_name);

        return (PyObject *)stmt_res;
    }
    else
    {
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_table_name);
        Py_XDECREF(py_column_name);

        Py_RETURN_FALSE;
    }
}

/*!# IfxPy.foreign_keys
*
* ===Description
* resource IfxPy.foreign_keys ( resource connection, string pk_qualifier,
* string pk_schema, string pk_table-name, string fk_qualifier
* string fk_schema, string fk_table-name )
*
* Returns a result set listing the foreign keys for a table.
*
* ===Parameters
*
* ====connection
*        A valid connection to an IDS database.
*
* ====pk_qualifier
*        A qualifier for the pk_table-name argument for the DB2 databases
* running on OS/390 or z/OS servers. For other databases, pass NULL or an empty
* string.
*
* ====pk_schema
*        The schema for the pk_table-name argument which contains the tables. If
* schema is NULL, IfxPy.foreign_keys() matches the schema for the current
* connection.
*
* ====pk_table-name
*        The name of the table which contains the primary key.
*
* ====fk_qualifier
*        A qualifier for the fk_table-name argument for the DB2 databases
* running on OS/390 or z/OS servers. For other databases, pass NULL or an empty
* string.
*
* ====fk_schema
*        The schema for the fk_table-name argument which contains the tables. If
* schema is NULL, IfxPy.foreign_keys() matches the schema for the current
* connection.
*
* ====fk_table-name
*        The name of the table which contains the foreign key.
*
* ===Return Values
*
* Returns a statement resource with a result set containing rows describing the
* foreign keys for the specified table. The result set is composed of the
* following columns:
*
* Column name::    Description
* PKTABLE_CAT:: Name of the catalog for the table containing the primary key.
* The value is NULL if this table does not have catalogs.
* PKTABLE_SCHEM:: Name of the schema for the table containing the primary key.
* PKTABLE_NAME:: Name of the table containing the primary key.
* PKCOLUMN_NAME:: Name of the column containing the primary key.
* FKTABLE_CAT:: Name of the catalog for the table containing the foreign key.
* The value is NULL if this table does not have catalogs.
* FKTABLE_SCHEM:: Name of the schema for the table containing the foreign key.
* FKTABLE_NAME:: Name of the table containing the foreign key.
* FKCOLUMN_NAME:: Name of the column containing the foreign key.
* KEY_SEQ:: 1-indexed position of the column in the key.
* UPDATE_RULE:: Integer value representing the action applied to the foreign
* key when the SQL operation is UPDATE.
* DELETE_RULE:: Integer value representing the action applied to the foreign
* key when the SQL operation is DELETE.
* FK_NAME:: The name of the foreign key.
* PK_NAME:: The name of the primary key.
* DEFERRABILITY:: An integer value representing whether the foreign key
* deferrability is SQL_INITIALLY_DEFERRED, SQL_INITIALLY_IMMEDIATE, or
* SQL_NOT_DEFERRABLE.
*/
static PyObject *IfxPy_foreign_keys(PyObject *self, PyObject *args)
{
    SQLWCHAR *pk_qualifier = NULL;
    SQLWCHAR *pk_owner = NULL;
    SQLWCHAR *pk_table_name = NULL;
    SQLWCHAR *fk_qualifier = NULL;
    SQLWCHAR *fk_owner = NULL;
    SQLWCHAR *fk_table_name = NULL;
    int rc;
    conn_handle *conn_res = NULL;
    stmt_handle *stmt_res;
    PyObject *py_conn_res = NULL;
    PyObject *py_pk_qualifier = NULL;
    PyObject *py_pk_owner = NULL;
    PyObject *py_pk_table_name = NULL;
    PyObject *py_fk_qualifier = NULL;
    PyObject *py_fk_owner = NULL;
    PyObject *py_fk_table_name = NULL;
    int isNewBuffer = 0;

    if (!PyArg_ParseTuple(args, "OOOO|OOO", &py_conn_res, &py_pk_qualifier,
        &py_pk_owner, &py_pk_table_name, &py_fk_qualifier,
        &py_fk_owner, &py_fk_table_name))
        return NULL;

    if (py_pk_qualifier != NULL && py_pk_qualifier != Py_None)
    {
        if (PyString_Check(py_pk_qualifier) || PyUnicode_Check(py_pk_qualifier))
        {
            py_pk_qualifier = PyUnicode_FromObject(py_pk_qualifier);
        }
        else
        {
            PyErr_SetString(PyExc_Exception,
                            "qualifier for table containing primary key must be a string or unicode");
            return NULL;
        }
    }

    if (py_pk_owner != NULL && py_pk_owner != Py_None)
    {
        if (PyString_Check(py_pk_owner) || PyUnicode_Check(py_pk_owner))
        {
            py_pk_owner = PyUnicode_FromObject(py_pk_owner);
        }
        else
        {
            PyErr_SetString(PyExc_Exception,
                            "owner of table containing primary key must be a string or unicode");
            Py_XDECREF(py_pk_qualifier);
            return NULL;
        }
    }

    if (py_pk_table_name != NULL && py_pk_table_name != Py_None)
    {
        if (PyString_Check(py_pk_table_name) || PyUnicode_Check(py_pk_table_name))
        {
            py_pk_table_name = PyUnicode_FromObject(py_pk_table_name);
        }
        else
        {
            PyErr_SetString(PyExc_Exception,
                            "name of the table that contains primary key must be a string or unicode");
            Py_XDECREF(py_pk_qualifier);
            Py_XDECREF(py_pk_owner);
            return NULL;
        }
    }

    if (py_fk_qualifier != NULL && py_fk_qualifier != Py_None)
    {
        if (PyString_Check(py_fk_qualifier) || PyUnicode_Check(py_fk_qualifier))
        {
            py_fk_qualifier = PyUnicode_FromObject(py_fk_qualifier);
        }
        else
        {
            PyErr_SetString(PyExc_Exception,
                            "qualifier for table containing the foreign key must be a string or unicode");
            Py_XDECREF(py_pk_qualifier);
            Py_XDECREF(py_pk_owner);
            Py_XDECREF(py_pk_table_name);
            return NULL;
        }
    }

    if (py_fk_owner != NULL && py_fk_owner != Py_None)
    {
        if (PyString_Check(py_fk_owner) || PyUnicode_Check(py_fk_owner))
        {
            py_fk_owner = PyUnicode_FromObject(py_fk_owner);
        }
        else
        {
            PyErr_SetString(PyExc_Exception,
                            "owner of table containing the foreign key must be a string or unicode");
            Py_XDECREF(py_pk_qualifier);
            Py_XDECREF(py_pk_owner);
            Py_XDECREF(py_pk_table_name);
            Py_XDECREF(py_fk_qualifier);
            return NULL;
        }
    }

    if (py_fk_table_name != NULL && py_fk_table_name != Py_None)
    {
        if (PyString_Check(py_fk_table_name) || PyUnicode_Check(py_fk_table_name))
        {
            py_fk_table_name = PyUnicode_FromObject(py_fk_table_name);
        }
        else
        {
            PyErr_SetString(PyExc_Exception,
                            "name of the table that contains foreign key must be a string or unicode");
            Py_XDECREF(py_pk_qualifier);
            Py_XDECREF(py_pk_owner);
            Py_XDECREF(py_pk_table_name);
            Py_XDECREF(py_fk_qualifier);
            Py_XDECREF(py_fk_owner);
            return NULL;
        }
    }

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }

        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            Py_XDECREF(py_pk_qualifier);
            Py_XDECREF(py_pk_owner);
            Py_XDECREF(py_pk_table_name);
            Py_XDECREF(py_fk_qualifier);
            Py_XDECREF(py_fk_owner);
            Py_XDECREF(py_fk_table_name);
            return NULL;
        }

        stmt_res = _IfxPy_new_stmt_struct(conn_res);

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLAllocHandle(SQL_HANDLE_STMT, conn_res->hdbc, &(stmt_res->hstmt));
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            Py_XDECREF(py_pk_qualifier);
            Py_XDECREF(py_pk_owner);
            Py_XDECREF(py_pk_table_name);
            Py_XDECREF(py_fk_qualifier);
            Py_XDECREF(py_fk_owner);
            Py_XDECREF(py_fk_table_name);

            Py_RETURN_FALSE;
        }

        if (py_pk_qualifier && py_pk_qualifier != Py_None)
            pk_qualifier = getUnicodeDataAsSQLWCHAR(py_pk_qualifier, &isNewBuffer);
        if (py_pk_owner && py_pk_owner != Py_None)
            pk_owner = getUnicodeDataAsSQLWCHAR(py_pk_owner, &isNewBuffer);
        if (py_pk_table_name && py_pk_table_name != Py_None)
            pk_table_name = getUnicodeDataAsSQLWCHAR(py_pk_table_name, &isNewBuffer);
        if (py_fk_qualifier && py_fk_qualifier != Py_None)
            fk_qualifier = getUnicodeDataAsSQLWCHAR(py_fk_qualifier, &isNewBuffer);
        if (py_fk_owner && py_fk_owner != Py_None)
            fk_owner = getUnicodeDataAsSQLWCHAR(py_fk_owner, &isNewBuffer);
        if (py_fk_table_name && py_fk_table_name != Py_None)
            fk_table_name = getUnicodeDataAsSQLWCHAR(py_fk_table_name, &isNewBuffer);

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLForeignKeysW((SQLHSTMT)stmt_res->hstmt, pk_qualifier, SQL_NTS,
                             pk_owner, SQL_NTS, pk_table_name, SQL_NTS, fk_qualifier, SQL_NTS,
                             fk_owner, SQL_NTS, fk_table_name, SQL_NTS);
        Py_END_ALLOW_THREADS;

        if (isNewBuffer)
        {
            if (pk_qualifier) PyMem_Del(pk_qualifier);
            if (pk_owner) PyMem_Del(pk_owner);
            if (pk_table_name) PyMem_Del(pk_table_name);
            if (fk_qualifier) PyMem_Del(fk_qualifier);
            if (fk_owner) PyMem_Del(fk_owner);
            if (fk_table_name) PyMem_Del(fk_table_name);
        }

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc,
                                            1, NULL, -1, 1);
            Py_XDECREF(py_pk_qualifier);
            Py_XDECREF(py_pk_owner);
            Py_XDECREF(py_pk_table_name);
            Py_XDECREF(py_fk_qualifier);
            Py_XDECREF(py_fk_owner);
            Py_XDECREF(py_fk_table_name);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        Py_XDECREF(py_pk_qualifier);
        Py_XDECREF(py_pk_owner);
        Py_XDECREF(py_pk_table_name);
        Py_XDECREF(py_fk_qualifier);
        Py_XDECREF(py_fk_owner);
        Py_XDECREF(py_fk_table_name);
        return (PyObject *)stmt_res;

    }
    else
    {
        Py_XDECREF(py_pk_qualifier);
        Py_XDECREF(py_pk_owner);
        Py_XDECREF(py_pk_table_name);
        Py_XDECREF(py_fk_qualifier);
        Py_XDECREF(py_fk_owner);
        Py_XDECREF(py_fk_table_name);
        Py_RETURN_FALSE;
    }
}

/*!# IfxPy.primary_keys
*
* ===Description
* resource IfxPy.primary_keys ( resource connection, string qualifier,
* string schema, string table-name )
*
* Returns a result set listing the primary keys for a table.
*
* ===Parameters
*
* ====connection
*        A valid connection to an IDS database.
*
* ====qualifier
*        A qualifier for DB2 databases running on OS/390 or z/OS servers. For
* other databases, pass NULL or an empty string.
*
* ====schema
*        The schema which contains the tables. If schema is NULL,
* IfxPy.primary_keys() matches the schema for the current connection.
*
* ====table-name
*        The name of the table.
*
* ===Return Values
*
* Returns a statement resource with a result set containing rows describing the
* primary keys for the specified table.
* The result set is composed of the following columns:
*
* Column name:: Description
* TABLE_CAT:: Name of the catalog for the table containing the primary key.
* The value is NULL if this table does not have catalogs.
* TABLE_SCHEM:: Name of the schema for the table containing the primary key.
* TABLE_NAME:: Name of the table containing the primary key.
* COLUMN_NAME:: Name of the column containing the primary key.
* KEY_SEQ:: 1-indexed position of the column in the key.
* PK_NAME:: The name of the primary key.
*/
static PyObject *IfxPy_primary_keys(PyObject *self, PyObject *args)
{
    SQLWCHAR *qualifier = NULL;
    SQLWCHAR *owner = NULL;
    SQLWCHAR *table_name = NULL;
    int rc;
    conn_handle *conn_res;
    stmt_handle *stmt_res;
    PyObject *py_conn_res = NULL;
    PyObject *py_qualifier = NULL;
    PyObject *py_owner = NULL;
    PyObject *py_table_name = NULL;
    int isNewBuffer;

    if (!PyArg_ParseTuple(args, "OOOO", &py_conn_res, &py_qualifier, &py_owner,
        &py_table_name))
        return NULL;

    if (py_qualifier != NULL && py_qualifier != Py_None)
    {
        if (PyString_Check(py_qualifier) || PyUnicode_Check(py_qualifier))
        {
            py_qualifier = PyUnicode_FromObject(py_qualifier);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "qualifier must be a string or unicode");
            return NULL;
        }
    }

    if (py_owner != NULL && py_owner != Py_None)
    {
        if (PyString_Check(py_owner) || PyUnicode_Check(py_owner))
        {
            py_owner = PyUnicode_FromObject(py_owner);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "owner must be a string or unicode");
            Py_XDECREF(py_qualifier);
            return NULL;
        }
    }

    if (py_table_name != NULL && py_table_name != Py_None)
    {
        if (PyString_Check(py_table_name) || PyUnicode_Check(py_table_name))
        {
            py_table_name = PyUnicode_FromObject(py_table_name);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "table_name must be a string or unicode");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            return NULL;
        }
    }

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }

        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            return NULL;
        }

        stmt_res = _IfxPy_new_stmt_struct(conn_res);

        rc = SQLAllocHandle(SQL_HANDLE_STMT, conn_res->hdbc, &(stmt_res->hstmt));
        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);

            Py_RETURN_FALSE;
        }
        if (py_qualifier && py_qualifier != Py_None)
            qualifier = getUnicodeDataAsSQLWCHAR(py_qualifier, &isNewBuffer);
        if (py_owner &&  py_owner != Py_None)
            owner = getUnicodeDataAsSQLWCHAR(py_owner, &isNewBuffer);
        if (py_table_name && py_table_name != Py_None)
            table_name = getUnicodeDataAsSQLWCHAR(py_table_name, &isNewBuffer);

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLPrimaryKeysW((SQLHSTMT)stmt_res->hstmt, qualifier, SQL_NTS,
                             owner, SQL_NTS, table_name, SQL_NTS);
        Py_END_ALLOW_THREADS;

        if (isNewBuffer)
        {
            if (qualifier) PyMem_Del(qualifier);
            if (owner) PyMem_Del(owner);
            if (table_name) PyMem_Del(table_name);
        }

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc,
                                            1, NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);

            Py_RETURN_FALSE;
        }
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_table_name);

        return (PyObject *)stmt_res;

    }
    else
    {
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_table_name);

        Py_RETURN_FALSE;
    }
}

/*!# IfxPy.procedure_columns
*
* ===Description
* resource IfxPy.procedure_columns ( resource connection, string qualifier,
* string schema, string procedure, string parameter )
*
* Returns a result set listing the parameters for one or more stored procedures
*
* ===Parameters
*
* ====connection
*        A valid connection to an IDS database.
*
* ====qualifier
*        A qualifier for DB2 databases running on OS/390 or z/OS servers. For
* other databases, pass NULL or an empty string.
*
* ====schema
*        The schema which contains the procedures. This parameter accepts a
* search pattern containing _ and % as wildcards.
*
* ====procedure
*        The name of the procedure. This parameter accepts a search pattern
* containing _ and % as wildcards.
*
* ====parameter
*        The name of the parameter. This parameter accepts a search pattern
* containing _ and % as wildcards.
*        If this parameter is NULL, all parameters for the specified stored
* procedures are returned.
*
* ===Return Values
*
* Returns a statement resource with a result set containing rows describing the
* parameters for the stored procedures matching the specified parameters. The
* rows are composed of the following columns:
*
* Column name::    Description
* PROCEDURE_CAT:: The catalog that contains the procedure. The value is NULL
* if this table does not have catalogs.
* PROCEDURE_SCHEM:: Name of the schema that contains the stored procedure.
* PROCEDURE_NAME:: Name of the procedure.
* COLUMN_NAME:: Name of the parameter.
* COLUMN_TYPE:: An integer value representing the type of the parameter:
*                      Return value:: Parameter type
*                      1:: (SQL_PARAM_INPUT)    Input (IN) parameter.
*                      2:: (SQL_PARAM_INPUT_OUTPUT) Input/output (INOUT)
*                          parameter.
*                      3:: (SQL_PARAM_OUTPUT) Output (OUT) parameter.
* DATA_TYPE:: The SQL data type for the parameter represented as an integer
* value.
* TYPE_NAME:: A string representing the data type for the parameter.
* COLUMN_SIZE:: An integer value representing the size of the parameter.
* BUFFER_LENGTH:: Maximum number of bytes necessary to store data for this
* parameter.
* DECIMAL_DIGITS:: The scale of the parameter, or NULL where scale is not
* applicable.
* NUM_PREC_RADIX:: An integer value of either 10 (representing an exact numeric
* data type), 2 (representing anapproximate numeric data type), or NULL
* (representing a data type for which radix is not applicable).
* NULLABLE:: An integer value representing whether the parameter is nullable or
* not.
* REMARKS:: Description of the parameter.
* COLUMN_DEF:: Default value for the parameter.
* SQL_DATA_TYPE:: An integer value representing the size of the parameter.
* SQL_DATETIME_SUB:: Returns an integer value representing a datetime subtype
* code, or NULL for SQL data types to which this does not apply.
* CHAR_OCTET_LENGTH:: Maximum length in octets for a character data type
* parameter, which matches COLUMN_SIZE for single-byte character set data, or
* NULL for non-character data types.
* ORDINAL_POSITION:: The 1-indexed position of the parameter in the CALL
* statement.
* IS_NULLABLE:: A string value where 'YES' means that the parameter accepts or
* returns NULL values and 'NO' means that the parameter does not accept or
* return NULL values.
*/
static PyObject *IfxPy_procedure_columns(PyObject *self, PyObject *args)
{
    SQLWCHAR *qualifier = NULL;
    SQLWCHAR *owner = NULL;
    SQLWCHAR *proc_name = NULL;
    SQLWCHAR *column_name = NULL;
    PyObject *py_qualifier = NULL;
    PyObject *py_owner = NULL;
    PyObject *py_proc_name = NULL;
    PyObject *py_column_name = NULL;
    PyObject *py_conn_res = NULL;
    int rc = 0;
    conn_handle *conn_res;
    stmt_handle *stmt_res;
    int isNewBuffer;

    if (!PyArg_ParseTuple(args, "O|OOOO", &py_conn_res, &py_qualifier, &py_owner,
        &py_proc_name, &py_column_name))
        return NULL;

    if (py_qualifier != NULL && py_qualifier != Py_None)
    {
        if (PyString_Check(py_qualifier) || PyUnicode_Check(py_qualifier))
        {
            py_qualifier = PyUnicode_FromObject(py_qualifier);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "qualifier must be a string or unicode");
            return NULL;
        }
    }

    if (py_owner != NULL && py_owner != Py_None)
    {
        if (PyString_Check(py_owner) || PyUnicode_Check(py_owner))
        {
            py_owner = PyUnicode_FromObject(py_owner);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "owner must be a string or unicode");
            Py_XDECREF(py_qualifier);
            return NULL;
        }
    }

    if (py_proc_name != NULL && py_proc_name != Py_None)
    {
        if (PyString_Check(py_proc_name) || PyUnicode_Check(py_proc_name))
        {
            py_proc_name = PyUnicode_FromObject(py_proc_name);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "table_name must be a string or unicode");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            return NULL;
        }
    }

    if (py_column_name != NULL && py_column_name != Py_None)
    {
        if (PyString_Check(py_column_name) || PyUnicode_Check(py_column_name))
        {
            py_column_name = PyUnicode_FromObject(py_column_name);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "column_name must be a string or unicode");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_proc_name);
            return NULL;
        }
    }

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }

        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_proc_name);
            Py_XDECREF(py_column_name);
            return NULL;
        }

        stmt_res = _IfxPy_new_stmt_struct(conn_res);

        rc = SQLAllocHandle(SQL_HANDLE_STMT, conn_res->hdbc, &(stmt_res->hstmt));
        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_proc_name);
            Py_XDECREF(py_column_name);

            Py_RETURN_FALSE;
        }
        if (py_qualifier && py_qualifier != Py_None)
            qualifier = getUnicodeDataAsSQLWCHAR(py_qualifier, &isNewBuffer);
        if (py_owner &&  py_owner != Py_None)
            owner = getUnicodeDataAsSQLWCHAR(py_owner, &isNewBuffer);
        if (py_proc_name && py_proc_name != Py_None)
            proc_name = getUnicodeDataAsSQLWCHAR(py_proc_name, &isNewBuffer);
        if (py_column_name && py_column_name != Py_None)
            column_name = getUnicodeDataAsSQLWCHAR(py_column_name, &isNewBuffer);

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLProcedureColumnsW((SQLHSTMT)stmt_res->hstmt, qualifier, SQL_NTS,
                                  owner, SQL_NTS, proc_name, SQL_NTS, column_name,
                                  SQL_NTS);
        Py_END_ALLOW_THREADS;

        if (isNewBuffer)
        {
            if (qualifier) PyMem_Del(qualifier);
            if (owner) PyMem_Del(owner);
            if (proc_name) PyMem_Del(proc_name);
            if (column_name) PyMem_Del(column_name);
        }

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc,
                                            1, NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_proc_name);
            Py_XDECREF(py_column_name);

            Py_RETURN_FALSE;
        }
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_proc_name);
        Py_XDECREF(py_column_name);

        return (PyObject *)stmt_res;
    }
    else
    {
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_proc_name);
        Py_XDECREF(py_column_name);

        Py_RETURN_FALSE;
    }
}

/*!# IfxPy.procedures
*
* ===Description
* resource IfxPy.procedures ( resource connection, string qualifier,
* string schema, string procedure )
*
* Returns a result set listing the stored procedures registered in a database.
*
* ===Parameters
*
* ====connection
*        A valid connection to an IDS database.
*
* ====qualifier
*        A qualifier for DB2 databases running on OS/390 or z/OS servers. For
* other databases, pass NULL or an empty string.
*
* ====schema
*        The schema which contains the procedures. This parameter accepts a
* search pattern containing _ and % as wildcards.
*
* ====procedure
*        The name of the procedure. This parameter accepts a search pattern
* containing _ and % as wildcards.
*
* ===Return Values
*
* Returns a statement resource with a result set containing rows describing the
* stored procedures matching the specified parameters. The rows are composed of
* the following columns:
*
* Column name:: Description
* PROCEDURE_CAT:: The catalog that contains the procedure. The value is NULL if
* this table does not have catalogs.
* PROCEDURE_SCHEM:: Name of the schema that contains the stored procedure.
* PROCEDURE_NAME:: Name of the procedure.
* NUM_INPUT_PARAMS:: Number of input (IN) parameters for the stored procedure.
* NUM_OUTPUT_PARAMS:: Number of output (OUT) parameters for the stored
* procedure.
* NUM_RESULT_SETS:: Number of result sets returned by the stored procedure.
* REMARKS:: Any comments about the stored procedure.
* PROCEDURE_TYPE:: Always returns 1, indicating that the stored procedure does
* not return a return value.
*/
static PyObject *IfxPy_procedures(PyObject *self, PyObject *args)
{
    SQLWCHAR *qualifier = NULL;
    SQLWCHAR *owner = NULL;
    SQLWCHAR *proc_name = NULL;
    int rc = 0;
    conn_handle *conn_res;
    stmt_handle *stmt_res;
    PyObject *py_conn_res = NULL;
    PyObject *py_qualifier = NULL;
    PyObject *py_owner = NULL;
    PyObject *py_proc_name = NULL;
    int isNewBuffer;

    if (!PyArg_ParseTuple(args, "OOOO", &py_conn_res, &py_qualifier, &py_owner,
        &py_proc_name))
        return NULL;

    if (py_qualifier != NULL && py_qualifier != Py_None)
    {
        if (PyString_Check(py_qualifier) || PyUnicode_Check(py_qualifier))
        {
            py_qualifier = PyUnicode_FromObject(py_qualifier);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "qualifier must be a string or unicode");
            return NULL;
        }
    }

    if (py_owner != NULL && py_owner != Py_None)
    {
        if (PyString_Check(py_owner) || PyUnicode_Check(py_owner))
        {
            py_owner = PyUnicode_FromObject(py_owner);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "owner must be a string or unicode");
            Py_XDECREF(py_qualifier);
            return NULL;
        }
    }

    if (py_proc_name != NULL && py_proc_name != Py_None)
    {
        if (PyString_Check(py_proc_name) || PyUnicode_Check(py_proc_name))
        {
            py_proc_name = PyUnicode_FromObject(py_proc_name);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "table_name must be a string or unicode");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            return NULL;
        }
    }


    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }

        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_proc_name);
            return NULL;
        }

        stmt_res = _IfxPy_new_stmt_struct(conn_res);

        rc = SQLAllocHandle(SQL_HANDLE_STMT, conn_res->hdbc, &(stmt_res->hstmt));
        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_proc_name);

            Py_RETURN_FALSE;
        }
        if (py_qualifier && py_qualifier != Py_None)
            qualifier = getUnicodeDataAsSQLWCHAR(py_qualifier, &isNewBuffer);
        if (py_owner &&  py_owner != Py_None)
            owner = getUnicodeDataAsSQLWCHAR(py_owner, &isNewBuffer);
        if (py_proc_name && py_proc_name != Py_None)
            proc_name = getUnicodeDataAsSQLWCHAR(py_proc_name, &isNewBuffer);

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLProceduresW((SQLHSTMT)stmt_res->hstmt, qualifier, SQL_NTS, owner,
                            SQL_NTS, proc_name, SQL_NTS);
        Py_END_ALLOW_THREADS;

        if (isNewBuffer)
        {
            if (qualifier) PyMem_Del(qualifier);
            if (owner) PyMem_Del(owner);
            if (proc_name) PyMem_Del(proc_name);
        }

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc,
                                            1, NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_proc_name);

            Py_RETURN_FALSE;
        }
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_proc_name);
        return (PyObject *)stmt_res;
    }
    else
    {
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_proc_name);

        Py_RETURN_FALSE;
    }
}

/*!# IfxPy.special_columns
*
* ===Description
* resource IfxPy.special_columns ( resource connection, string qualifier,
* string schema, string table_name, int scope )
*
* Returns a result set listing the unique row identifier columns for a table.
*
* ===Parameters
*
* ====connection
*        A valid connection to an IDS database.
*
* ====qualifier
*        A qualifier for DB2 databases running on OS/390 or z/OS servers. For
* other databases, pass NULL or an empty string.
*
* ====schema
*        The schema which contains the tables.
*
* ====table_name
*        The name of the table.
*
* ====scope
*        Integer value representing the minimum duration for which the unique
* row identifier is valid. This can be one of the following values:
*
*        0: Row identifier is valid only while the cursor is positioned on the
* row. (SQL_SCOPE_CURROW)
*        1: Row identifier is valid for the duration of the transaction.
* (SQL_SCOPE_TRANSACTION)
*        2: Row identifier is valid for the duration of the connection.
* (SQL_SCOPE_SESSION)
*
* ===Return Values
*
* Returns a statement resource with a result set containing rows with unique
* row identifier information for a table.
* The rows are composed of the following columns:
*
* Column name:: Description
*
* SCOPE:: Integer value representing the minimum duration for which the unique
* row identifier is valid.
*
*             0: Row identifier is valid only while the cursor is positioned on
* the row. (SQL_SCOPE_CURROW)
*
*             1: Row identifier is valid for the duration of the transaction.
* (SQL_SCOPE_TRANSACTION)
*
*             2: Row identifier is valid for the duration of the connection.
* (SQL_SCOPE_SESSION)
*
* COLUMN_NAME:: Name of the unique column.
*
* DATA_TYPE:: SQL data type for the column.
*
* TYPE_NAME:: Character string representation of the SQL data type for the
* column.
*
* COLUMN_SIZE:: An integer value representing the size of the column.
*
* BUFFER_LENGTH:: Maximum number of bytes necessary to store data from this
* column.
*
* DECIMAL_DIGITS:: The scale of the column, or NULL where scale is not
* applicable.
*
* NUM_PREC_RADIX:: An integer value of either 10 (representing an exact numeric
* data type), 2 (representing an approximate numeric data type), or NULL
* (representing a data type for which radix is not applicable).
*
* PSEUDO_COLUMN:: Always returns 1.
*/
static PyObject *IfxPy_special_columns(PyObject *self, PyObject *args)
{
    SQLWCHAR *qualifier = NULL;
    SQLWCHAR *owner = NULL;
    SQLWCHAR *table_name = NULL;
    int scope = 0;
    conn_handle *conn_res;
    stmt_handle *stmt_res;
    int rc;
    PyObject *py_conn_res = NULL;
    PyObject *py_scope = NULL;
    PyObject *py_qualifier = NULL;
    PyObject *py_owner = NULL;
    PyObject *py_table_name = NULL;
    int isNewBuffer;

    if (!PyArg_ParseTuple(args, "OOOOO", &py_conn_res, &py_qualifier, &py_owner,
        &py_table_name, &py_scope))
        return NULL;

    if (!NIL_P(py_scope))
    {
        if (PyInt_Check(py_scope))
        {
            scope = (int)PyInt_AsLong(py_scope);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
            return NULL;
        }
    }
    if (py_qualifier != NULL && py_qualifier != Py_None)
    {
        if (PyString_Check(py_qualifier) || PyUnicode_Check(py_qualifier))
        {
            py_qualifier = PyUnicode_FromObject(py_qualifier);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "qualifier must be a string or unicode");
            return NULL;
        }
    }

    if (py_owner != NULL && py_owner != Py_None)
    {
        if (PyString_Check(py_owner) || PyUnicode_Check(py_owner))
        {
            py_owner = PyUnicode_FromObject(py_owner);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "owner must be a string or unicode");
            Py_XDECREF(py_qualifier);
            return NULL;
        }
    }

    if (py_table_name != NULL && py_table_name != Py_None)
    {
        if (PyString_Check(py_table_name) || PyUnicode_Check(py_table_name))
        {
            py_table_name = PyUnicode_FromObject(py_table_name);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "table_name must be a string or unicode");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            return NULL;
        }
    }

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }

        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            return NULL;
        }

        stmt_res = _IfxPy_new_stmt_struct(conn_res);

        rc = SQLAllocHandle(SQL_HANDLE_STMT, conn_res->hdbc, &(stmt_res->hstmt));
        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);

            Py_RETURN_FALSE;
        }
        if (py_qualifier && py_qualifier != Py_None)
            qualifier = getUnicodeDataAsSQLWCHAR(py_qualifier, &isNewBuffer);
        if (py_owner &&  py_owner != Py_None)
            owner = getUnicodeDataAsSQLWCHAR(py_owner, &isNewBuffer);
        if (py_table_name && py_table_name != Py_None)
            table_name = getUnicodeDataAsSQLWCHAR(py_table_name, &isNewBuffer);

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLSpecialColumnsW((SQLHSTMT)stmt_res->hstmt, SQL_BEST_ROWID,
                                qualifier, SQL_NTS, owner, SQL_NTS, table_name,
                                SQL_NTS, scope, SQL_NULLABLE);
        Py_END_ALLOW_THREADS;

        if (isNewBuffer)
        {
            if (qualifier) PyMem_Del(qualifier);
            if (owner) PyMem_Del(owner);
            if (table_name) PyMem_Del(table_name);
        }

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc,
                                            1, NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);

            Py_RETURN_FALSE;
        }
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_table_name);

        return (PyObject *)stmt_res;
    }
    else
    {
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_table_name);

        Py_RETURN_FALSE;

    }
}

/*!# IfxPy.statistics
*
* ===Description
* resource IfxPy.statistics ( resource connection, string qualifier,
* string schema, string table-name, bool unique )
*
* Returns a result set listing the index and statistics for a table.
*
* ===Parameters
*
* ====connection
*        A valid connection to an IDS database.
*
* ====qualifier
*        A qualifier for DB2 databases running on OS/390 or z/OS servers. For
* other databases, pass NULL or an empty string.
*
* ====schema
*        The schema that contains the targeted table. If this parameter is NULL,
* the statistics and indexes are returned for the schema of the current user.
*
* ====table_name
*        The name of the table.
*
* ====unique
*        A boolean value representing the type of index information to return.
*
*        False     Return only the information for unique indexes on the table.
*
*        True      Return the information for all indexes on the table.
*
* ===Return Values
*
* Returns a statement resource with a result set containing rows describing the
* statistics and indexes for the base tables matching the specified parameters.
* The rows are composed of the following columns:
*
* Column name:: Description
* TABLE_CAT:: The catalog that contains the table. The value is NULL if this
* table does not have catalogs.
* TABLE_SCHEM:: Name of the schema that contains the table.
* TABLE_NAME:: Name of the table.
* NON_UNIQUE:: An integer value representing whether the index prohibits unique
* values, or whether the row represents statistics on the table itself:
*
*                     Return value:: Parameter type
*                     0 (SQL_FALSE):: The index allows duplicate values.
*                     1 (SQL_TRUE):: The index values must be unique.
*                     NULL:: This row is statistics information for the table
*                     itself.
*
* INDEX_QUALIFIER:: A string value representing the qualifier that would have
* to be prepended to INDEX_NAME to fully qualify the index.
* INDEX_NAME:: A string representing the name of the index.
* TYPE:: An integer value representing the type of information contained in
* this row of the result set:
*
*            Return value:: Parameter type
*            0 (SQL_TABLE_STAT):: The row contains statistics about the table
*                                 itself.
*            1 (SQL_INDEX_CLUSTERED):: The row contains information about a
*                                      clustered index.
*            2 (SQL_INDEX_HASH):: The row contains information about a hashed
*                                 index.
*            3 (SQL_INDEX_OTHER):: The row contains information about a type of
* index that is neither clustered nor hashed.
*
* ORDINAL_POSITION:: The 1-indexed position of the column in the index. NULL if
* the row contains statistics information about the table itself.
* COLUMN_NAME:: The name of the column in the index. NULL if the row contains
* statistics information about the table itself.
* ASC_OR_DESC:: A if the column is sorted in ascending order, D if the column
* is sorted in descending order, NULL if the row contains statistics
* information about the table itself.
* CARDINALITY:: If the row contains information about an index, this column
* contains an integer value representing the number of unique values in the
* index. If the row contains information about the table itself, this column
* contains an integer value representing the number of rows in the table.
* PAGES:: If the row contains information about an index, this column contains
* an integer value representing the number of pages used to store the index. If
* the row contains information about the table itself, this column contains an
* integer value representing the number of pages used to store the table.
* FILTER_CONDITION:: Always returns NULL.
*/
static PyObject *IfxPy_statistics(PyObject *self, PyObject *args)
{
    SQLWCHAR *qualifier = NULL;
    SQLWCHAR *owner = NULL;
    SQLWCHAR *table_name = NULL;
    int unique = 0;
    int rc = 0;
    SQLUSMALLINT sql_unique;
    conn_handle *conn_res;
    stmt_handle *stmt_res;
    PyObject *py_conn_res = NULL;
    PyObject *py_qualifier = NULL;
    PyObject *py_owner = NULL;
    PyObject *py_table_name = NULL;
    PyObject *py_unique = NULL;
    int isNewBuffer;

    if (!PyArg_ParseTuple(args, "OOOOO", &py_conn_res, &py_qualifier, &py_owner,
        &py_table_name, &py_unique))
        return NULL;

    if (py_qualifier != NULL && py_qualifier != Py_None)
    {
        if (PyString_Check(py_qualifier) || PyUnicode_Check(py_qualifier))
        {
            py_qualifier = PyUnicode_FromObject(py_qualifier);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "qualifier must be a string or unicode");
            return NULL;
        }
    }

    if (py_owner != NULL && py_owner != Py_None)
    {
        if (PyString_Check(py_owner) || PyUnicode_Check(py_owner))
        {
            py_owner = PyUnicode_FromObject(py_owner);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "owner must be a string or unicode");
            Py_XDECREF(py_qualifier);
            return NULL;
        }
    }

    if (py_table_name != NULL && py_table_name != Py_None)
    {
        if (PyString_Check(py_table_name) || PyUnicode_Check(py_table_name))
        {
            py_table_name = PyUnicode_FromObject(py_table_name);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "table_name must be a string or unicode");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            return NULL;
        }
    }

    if (py_unique != NULL && py_unique != Py_None)
    {
        if (PyBool_Check(py_unique))
        {
            if (py_unique == Py_True)
                unique = 1;
            else
                unique = 0;
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "unique must be a boolean");
            return NULL;
        }
    }

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }

        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            return NULL;
        }

        stmt_res = _IfxPy_new_stmt_struct(conn_res);
        sql_unique = unique;

        rc = SQLAllocHandle(SQL_HANDLE_STMT, conn_res->hdbc, &(stmt_res->hstmt));
        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);

            Py_RETURN_FALSE;
        }
        if (py_qualifier && py_qualifier != Py_None)
            qualifier = getUnicodeDataAsSQLWCHAR(py_qualifier, &isNewBuffer);
        if (py_owner &&  py_owner != Py_None)
            owner = getUnicodeDataAsSQLWCHAR(py_owner, &isNewBuffer);
        if (py_table_name && py_table_name != Py_None)
            table_name = getUnicodeDataAsSQLWCHAR(py_table_name, &isNewBuffer);

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLStatisticsW((SQLHSTMT)stmt_res->hstmt, qualifier, SQL_NTS, owner,
                            SQL_NTS, table_name, SQL_NTS, sql_unique, SQL_QUICK);
        Py_END_ALLOW_THREADS;

        if (isNewBuffer)
        {
            if (qualifier) PyMem_Del(qualifier);
            if (owner) PyMem_Del(owner);
            if (table_name) PyMem_Del(table_name);
        }

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc,
                                            1, NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);

            Py_RETURN_FALSE;
        }
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_table_name);

        return (PyObject *)stmt_res;
    }
    else
    {
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_table_name);

        Py_RETURN_FALSE;
    }
}

/*!# IfxPy.table_privileges
*
* ===Description
* resource IfxPy.table_privileges ( resource connection [, string qualifier
* [, string schema [, string table_name]]] )
*
* Returns a result set listing the tables and associated privileges in a
* database.
*
* ===Parameters
*
* ====connection
*        A valid connection to an IDS database.
*
* ====qualifier
*        A qualifier for DB2 databases running on OS/390 or z/OS servers. For
* other databases, pass NULL or an empty string.
*
* ====schema
*        The schema which contains the tables. This parameter accepts a search
* pattern containing _ and % as wildcards.
*
* ====table_name
*        The name of the table. This parameter accepts a search pattern
* containing _ and % as wildcards.
*
* ===Return Values
*
* Returns a statement resource with a result set containing rows describing
* the privileges for the tables that match the specified parameters. The rows
* are composed of the following columns:
*
* Column name:: Description
* TABLE_CAT:: The catalog that contains the table. The value is NULL if this
* table does not have catalogs.
* TABLE_SCHEM:: Name of the schema that contains the table.
* TABLE_NAME:: Name of the table.
* GRANTOR:: Authorization ID of the user who granted the privilege.
* GRANTEE:: Authorization ID of the user to whom the privilege was granted.
* PRIVILEGE:: The privilege that has been granted. This can be one of ALTER,
* CONTROL, DELETE, INDEX, INSERT, REFERENCES, SELECT, or UPDATE.
* IS_GRANTABLE:: A string value of "YES" or "NO" indicating whether the grantee
* can grant the privilege to other users.
*/
static PyObject *IfxPy_table_privileges(PyObject *self, PyObject *args)
{
    SQLWCHAR *qualifier = NULL;
    SQLWCHAR *owner = NULL;
    SQLWCHAR *table_name = NULL;
    conn_handle *conn_res;
    stmt_handle *stmt_res;
    int rc;
    PyObject *py_conn_res = NULL;
    PyObject *py_qualifier = NULL;
    PyObject *py_owner = NULL;
    PyObject *py_table_name = NULL;
    int isNewBuffer;

    if (!PyArg_ParseTuple(args, "O|OOO", &py_conn_res, &py_qualifier, &py_owner,
        &py_table_name))
        return NULL;

    if (py_qualifier != NULL && py_qualifier != Py_None)
    {
        if (PyString_Check(py_qualifier) || PyUnicode_Check(py_qualifier))
        {
            py_qualifier = PyUnicode_FromObject(py_qualifier);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "qualifier must be a string or unicode");
            return NULL;
        }
    }

    if (py_owner != NULL && py_owner != Py_None)
    {
        if (PyString_Check(py_owner) || PyUnicode_Check(py_owner))
        {
            py_owner = PyUnicode_FromObject(py_owner);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "owner must be a string or unicode");
            Py_XDECREF(py_qualifier);
            return NULL;
        }
    }

    if (py_table_name != NULL && py_table_name != Py_None)
    {
        if (PyString_Check(py_table_name) || PyUnicode_Check(py_table_name))
        {
            py_table_name = PyUnicode_FromObject(py_table_name);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "table_name must be a string or unicode");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            return NULL;
        }
    }

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }

        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            return NULL;
        }

        if (!conn_res)
        {
            PyErr_SetString(PyExc_Exception, "Connection Resource cannot be found");
            Py_RETURN_FALSE;
        }

        stmt_res = _IfxPy_new_stmt_struct(conn_res);

        rc = SQLAllocHandle(SQL_HANDLE_STMT, conn_res->hdbc, &(stmt_res->hstmt));
        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);

            Py_RETURN_FALSE;
        }
        if (py_qualifier && py_qualifier != Py_None)
            qualifier = getUnicodeDataAsSQLWCHAR(py_qualifier, &isNewBuffer);
        if (py_owner &&  py_owner != Py_None)
            owner = getUnicodeDataAsSQLWCHAR(py_owner, &isNewBuffer);
        if (py_table_name && py_table_name != Py_None)
            table_name = getUnicodeDataAsSQLWCHAR(py_table_name, &isNewBuffer);

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLTablePrivilegesW((SQLHSTMT)stmt_res->hstmt, qualifier, SQL_NTS,
                                 owner, SQL_NTS, table_name, SQL_NTS);
        Py_END_ALLOW_THREADS;

        if (isNewBuffer)
        {
            if (qualifier) PyMem_Del(qualifier);
            if (owner) PyMem_Del(owner);
            if (table_name) PyMem_Del(table_name);
        }

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc,
                                            1, NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);

            Py_RETURN_FALSE;
        }
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_table_name);

        return (PyObject *)stmt_res;
    }
    else
    {
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_table_name);

        Py_RETURN_FALSE;
    }
}

/*!# IfxPy.tables
*
* ===Description
* resource IfxPy.tables ( resource connection [, string qualifier [, string
* schema [, string table-name [, string table-type]]]] )
*
* Returns a result set listing the tables and associated metadata in a database
*
* ===Parameters
*
* ====connection
*        A valid connection to an IDS database.
*
* ====qualifier
*        A qualifier for DB2 databases running on OS/390 or z/OS servers. For
* other databases, pass NULL or an empty string.
*
* ====schema
*        The schema which contains the tables. This parameter accepts a search
* pattern containing _ and % as wildcards.
*
* ====table-name
*        The name of the table. This parameter accepts a search pattern
* containing _ and % as wildcards.
*
* ====table-type
*        A list of comma-delimited table type identifiers. To match all table
* types, pass NULL or an empty string.
*        Valid table type identifiers include: ALIAS, HIERARCHY TABLE,
* INOPERATIVE VIEW, NICKNAME, MATERIALIZED QUERY TABLE, SYSTEM TABLE, TABLE,
* TYPED TABLE, TYPED VIEW, and VIEW.
*
* ===Return Values
*
* Returns a statement resource with a result set containing rows describing
* the tables that match the specified parameters.
* The rows are composed of the following columns:
*
* Column name:: Description
* TABLE_CAT:: The catalog that contains the table. The value is NULL if this
* table does not have catalogs.
* TABLE_SCHEMA:: Name of the schema that contains the table.
* TABLE_NAME:: Name of the table.
* TABLE_TYPE:: Table type identifier for the table.
* REMARKS:: Description of the table.
*/
static PyObject *IfxPy_tables(PyObject *self, PyObject *args)
{
    SQLWCHAR *qualifier = NULL;
    SQLWCHAR *owner = NULL;
    SQLWCHAR *table_name = NULL;
    SQLWCHAR *table_type = NULL;
    PyObject *py_qualifier = NULL;
    PyObject *py_owner = NULL;
    PyObject *py_table_name = NULL;
    PyObject *py_table_type = NULL;
    PyObject *py_conn_res;
    conn_handle *conn_res;
    stmt_handle *stmt_res;
    int rc;
    int isNewBuffer;

    if (!PyArg_ParseTuple(args, "O|OOOO", &py_conn_res, &py_qualifier, &py_owner,
        &py_table_name, &py_table_type))
        return NULL;

    if (py_qualifier != NULL && py_qualifier != Py_None)
    {
        if (PyString_Check(py_qualifier) || PyUnicode_Check(py_qualifier))
        {
            py_qualifier = PyUnicode_FromObject(py_qualifier);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "qualifier must be a string or unicode");
            return NULL;
        }
    }

    if (py_owner != NULL && py_owner != Py_None)
    {
        if (PyString_Check(py_owner) || PyUnicode_Check(py_owner))
        {
            py_owner = PyUnicode_FromObject(py_owner);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "owner must be a string or unicode");
            Py_XDECREF(py_qualifier);
            return NULL;
        }
    }

    if (py_table_name != NULL && py_table_name != Py_None)
    {
        if (PyString_Check(py_table_name) || PyUnicode_Check(py_table_name))
        {
            py_table_name = PyUnicode_FromObject(py_table_name);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "table_name must be a string or unicode");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            return NULL;
        }
    }

    if (py_table_type != NULL && py_table_type != Py_None)
    {
        if (PyString_Check(py_table_type) || PyUnicode_Check(py_table_type))
        {
            py_table_type = PyUnicode_FromObject(py_table_type);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "table type must be a string or unicode");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            return NULL;
        }
    }

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }

        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            Py_XDECREF(py_table_type);
            return NULL;
        }

        stmt_res = _IfxPy_new_stmt_struct(conn_res);

        rc = SQLAllocHandle(SQL_HANDLE_STMT, conn_res->hdbc, &(stmt_res->hstmt));
        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            Py_XDECREF(py_table_type);

            Py_RETURN_FALSE;
        }
        if (py_qualifier && py_qualifier != Py_None)
            qualifier = getUnicodeDataAsSQLWCHAR(py_qualifier, &isNewBuffer);
        if (py_owner &&  py_owner != Py_None)
            owner = getUnicodeDataAsSQLWCHAR(py_owner, &isNewBuffer);
        if (py_table_name && py_table_name != Py_None)
            table_name = getUnicodeDataAsSQLWCHAR(py_table_name, &isNewBuffer);
        if (py_table_type && py_table_type != Py_None)
            table_type = getUnicodeDataAsSQLWCHAR(py_table_type, &isNewBuffer);

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLTablesW((SQLHSTMT)stmt_res->hstmt, qualifier, SQL_NTS, owner,
                        SQL_NTS, table_name, SQL_NTS, table_type, SQL_NTS);
        Py_END_ALLOW_THREADS;

        if (isNewBuffer)
        {
            if (qualifier) PyMem_Del(qualifier);
            if (owner) PyMem_Del(owner);
            if (table_name) PyMem_Del(table_name);
            if (table_type) PyMem_Del(table_type);
        }

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc,
                                            1, NULL, -1, 1);
            Py_XDECREF(py_qualifier);
            Py_XDECREF(py_owner);
            Py_XDECREF(py_table_name);
            Py_XDECREF(py_table_type);

            Py_RETURN_FALSE;
        }
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_table_name);
        Py_XDECREF(py_table_type);

        return (PyObject *)stmt_res;
    }
    else
    {
        Py_XDECREF(py_qualifier);
        Py_XDECREF(py_owner);
        Py_XDECREF(py_table_name);
        Py_XDECREF(py_table_type);

        Py_RETURN_FALSE;
    }
}

/*!# IfxPy.commit
* ===Description
* bool IfxPy.commit ( resource connection )
*
* Commits an in-progress transaction on the specified connection resource and
* begins a new transaction.
* Python applications normally default to AUTOCOMMIT mode, so IfxPy.commit()
* is not necessary unless AUTOCOMMIT has been turned off for the connection
* resource.
*
* Note: If the specified connection resource is a persistent connection, all
* transactions in progress for all applications using that persistent
* connection will be committed. For this reason, persistent connections are
* not recommended for use in applications that require transactions.
*
* ===Parameters
*
* ====connection
*        A valid database connection resource variable as returned from
* IfxPy.connect() 
*
* ===Return Values
*
* Returns TRUE on success or FALSE on failure.
*/
static PyObject *IfxPy_commit(PyObject *self, PyObject *args)
{
    PyObject *py_conn_res = NULL;
    conn_handle *conn_res;
    int rc;

    if (!PyArg_ParseTuple(args, "O", &py_conn_res))
        return NULL;

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }

        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            return NULL;
        }

        rc = SQLEndTran(SQL_HANDLE_DBC, conn_res->hdbc, SQL_COMMIT);

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            Py_INCREF(Py_False);
            return Py_False;
        }
        else
        {
            Py_INCREF(Py_True);
            return Py_True;
        }
    }
    Py_INCREF(Py_False);
    return Py_False;
}

// static int _python_IfxPy_do_prepare(SQLHANDLE hdbc, char *stmt_string, stmt_handle *stmt_res, PyObject *options)
static int _python_IfxPy_do_prepare(SQLHANDLE hdbc, SQLWCHAR *stmt, int stmt_size, stmt_handle *stmt_res, PyObject *options)
{
    int rc;

    /* alloc handle and return only if it errors */
    rc = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &(stmt_res->hstmt));
    if (rc == SQL_ERROR)
    {
        _python_IfxPy_check_sql_errors(hdbc, SQL_HANDLE_DBC, rc,
                                        1, NULL, -1, 1);
        return rc;
    }

    /* get the string and its length */
    if (NIL_P(stmt))
    {
        PyErr_SetString(PyExc_Exception,
                        "Supplied statement parameter is invalid");
        return rc;
    }

    if (rc < SQL_SUCCESS)
    {
        _python_IfxPy_check_sql_errors(hdbc, SQL_HANDLE_DBC, rc, 1, NULL, -1, 1);
        PyErr_SetString(PyExc_Exception, "Statement prepare Failed: ");
        return rc;
    }

    if (!NIL_P(options))
    {
        rc = _python_IfxPy_parse_options(options, SQL_HANDLE_STMT, stmt_res);
        if (rc == SQL_ERROR)
        {
            return rc;
        }
    }

    // Prepare the stmt. The cursor type requested has already been set in
    // _python_IfxPy_assign_options
    Py_BEGIN_ALLOW_THREADS;
    rc = SQLPrepareW((SQLHSTMT)stmt_res->hstmt, stmt,
                     stmt_size);
    Py_END_ALLOW_THREADS;

    if (rc == SQL_ERROR)
    {
        _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc,
                                        1, NULL, -1, 1);
    }
    return rc;
}


/*!# IfxPy.exec
*
* ===Description
* stmt_handle IfxPy.exec ( IFXConnection connection, string statement
*                                [, array options] )
*
* Prepares and executes an SQL statement.
*
* If you plan to interpolate Python variables into the SQL statement,
* understand that this is one of the more common security exposures. Consider
* calling IfxPy.prepare() to prepare an SQL statement with parameter markers    * for input values. Then you can call IfxPy.execute() to pass in the input
* values and avoid SQL injection attacks.
*
* If you plan to repeatedly issue the same SQL statement with different
* parameters, consider calling IfxPy.:prepare() and IfxPy.execute() to
* enable the database server to reuse its access plan and increase the
* efficiency of your database access.
*
* ===Parameters
*
* ====connection
*
*        A valid database connection resource variable as returned from
* IfxPy.connect() 
*
* ====statement
*
*        An SQL statement. The statement cannot contain any parameter markers.
*
* ====options
*
*        An dictionary containing statement options. You can use this parameter  * to request a scrollable cursor on database servers that support this
* functionality.
*
*        SQL_ATTR_CURSOR_TYPE
*             Passing the SQL_SCROLL_FORWARD_ONLY value requests a forward-only
*             cursor for this SQL statement. This is the default type of
*             cursor, and it is supported by all database servers. It is also
*             much faster than a scrollable cursor.
*
*             Passing the SQL_CURSOR_KEYSET_DRIVEN value requests a scrollable  *             cursor for this SQL statement. This type of cursor enables you to
*             fetch rows non-sequentially from the database server. However, it
*             is only supported by DB2 servers, and is much slower than
*             forward-only cursors.
*
* ===Return Values
*
* Returns a stmt_handle resource if the SQL statement was issued
* successfully, or FALSE if the database failed to execute the SQL statement.
*/
static PyObject *IfxPy_exec(PyObject *self, PyObject *args)
{
    PyObject *options = NULL;
    PyObject *py_conn_res = NULL;
    stmt_handle *stmt_res;
    conn_handle *conn_res;
    int rc;
    int isNewBuffer;
    char* return_str = NULL; // This variable is used by _python_IfxPy_check_sql_errors to return err strings
    SQLWCHAR *stmt = NULL;
    PyObject *py_stmt = NULL;

    // This function basically is a wrap of the _python_IfxPy_do_prepare and
    //_python_IfxPy_Execute_stmt
    //After completing statement execution, it returns the statement resource


    if (!PyArg_ParseTuple(args, "OO|O", &py_conn_res, &py_stmt, &options))
        return NULL;

    if (py_stmt != NULL && py_stmt != Py_None)
    {
        if (PyString_Check(py_stmt) || PyUnicode_Check(py_stmt))
        {
            py_stmt = PyUnicode_FromObject(py_stmt);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "statement must be a string or unicode");
            return NULL;
        }
    }

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }

        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            Py_XDECREF(py_stmt);
            return NULL;
        }

        return_str = ALLOC_N(char, DB_MAX_ERR_MSG_LEN);
        if (return_str == NULL)
        {
            PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
            Py_XDECREF(py_stmt);
            return NULL;
        }

        memset(return_str, 0, DB_MAX_ERR_MSG_LEN);

        _python_IfxPy_clear_stmt_err_cache();

        stmt_res = _IfxPy_new_stmt_struct(conn_res);

        /* Allocates the stmt handle */
        /* returns the stat_handle back to the calling function */
        rc = SQLAllocHandle(SQL_HANDLE_STMT, conn_res->hdbc, &(stmt_res->hstmt));
        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyMem_Del(return_str);
            Py_XDECREF(py_stmt);
            return NULL;
        }

        if (!NIL_P(options))
        {
            rc = _python_IfxPy_parse_options(options, SQL_HANDLE_STMT, stmt_res);
            if (rc == SQL_ERROR)
            {
                Py_XDECREF(py_stmt);
                return NULL;
            }
        }
        if (py_stmt != NULL && py_stmt != Py_None)
        {
            stmt = getUnicodeDataAsSQLWCHAR(py_stmt, &isNewBuffer);
        }

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLExecDirectW((SQLHSTMT)stmt_res->hstmt, stmt, SQL_NTS);
        Py_END_ALLOW_THREADS;
        if (rc < SQL_SUCCESS)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, -1,
                                            1, return_str, IDS_ERRMSG,
                                            stmt_res->errormsg_recno_tracker);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_res->hstmt);
            /* TODO: Object freeing */
            /* free(stmt_res); */
            if (isNewBuffer)
            {
                if (stmt) PyMem_Del(stmt);
            }
            Py_XDECREF(py_stmt);
            PyMem_Del(return_str);
            return NULL;
        }
        if (rc == SQL_SUCCESS_WITH_INFO)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, 1,
                                            1, return_str, IDS_WARNMSG,
                                            stmt_res->errormsg_recno_tracker);
        }
        if (isNewBuffer)
        {
            if (stmt) PyMem_Del(stmt);
        }
        PyMem_Del(return_str);
        Py_XDECREF(py_stmt);
        return (PyObject *)stmt_res;
    }
    Py_XDECREF(py_stmt);
    return NULL;
}

/*!# IfxPy.free_result
*
* ===Description
* bool IfxPy.free_result ( resource stmt )
*
* Frees the system and database resources that are associated with a result
* set. These resources are freed implicitly when a script finishes, but you
* can call IfxPy.free_result() to explicitly free the result set resources
* before the end of the script.
*
* ===Parameters
*
* ====stmt
*        A valid statement resource.
*
* ===Return Values
*
* Returns TRUE on success or FALSE on failure.
*/
static PyObject *IfxPy_free_result(PyObject *self, PyObject *args)
{
    PyObject *py_stmt_res = NULL;
    stmt_handle *stmt_res;
    int rc = 0;

    if (!PyArg_ParseTuple(args, "O", &py_stmt_res))
        return NULL;

    if (!NIL_P(py_stmt_res))
    {
        if (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
            return NULL;
        }
        else
        {
            stmt_res = (stmt_handle *)py_stmt_res;
        }
        if (stmt_res->hstmt)
        {
            // Free any cursors that might have been allocated in a previous call
            //to SQLExecute

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLFreeStmt((SQLHSTMT)stmt_res->hstmt, SQL_CLOSE);
            Py_END_ALLOW_THREADS;
            if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT,
                                                rc, 1, NULL, -1, 1);
            }
            if (rc == SQL_ERROR)
            {
                PyErr_Clear();
                Py_RETURN_FALSE;
            }
        }
        _python_IfxPy_free_result_struct(stmt_res);
    }
    else
    {
        PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
        return NULL;
    }
    Py_INCREF(Py_True);
    return Py_True;
}


static PyObject *_python_IfxPy_prepare_helper(conn_handle *conn_res, PyObject *py_stmt, PyObject *options)
{
    stmt_handle *stmt_res;
    int rc;
    char error [DB_MAX_ERR_MSG_LEN];
    SQLWCHAR *stmt = NULL;
    int stmt_size = 0;
    int isNewBuffer;

    if (!conn_res->handle_active)
    {
        PyErr_SetString(PyExc_Exception, "Connection is not active");
        return NULL;
    }

    if (py_stmt != NULL && py_stmt != Py_None)
    {
        if (PyString_Check(py_stmt) || PyUnicode_Check(py_stmt))
        {
            py_stmt = PyUnicode_FromObject(py_stmt);
            if (py_stmt != NULL &&  py_stmt != Py_None)
            {
#if PY_MAJOR_VERSION >=3 && PY_MINOR_VERSION >= 3
                stmt_size = (int)PyUnicode_GetLength(py_stmt);
#else
                stmt_size = (int)PyUnicode_GetSize(py_stmt);
#endif
            }
            else
            {
                PyErr_SetString(PyExc_Exception, "Error occure during processing of statement");
                return NULL;
            }
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "statement must be a string or unicode");
            return NULL;
        }
    }

    _python_IfxPy_clear_stmt_err_cache();

    /* Initialize stmt resource members with default values. */
    /* Parsing will update options if needed */

    stmt_res = _IfxPy_new_stmt_struct(conn_res);

    /* Allocates the stmt handle */
    /* Prepares the statement */
    /* returns the stat_handle back to the calling function */
    if (py_stmt && py_stmt != Py_None)
        stmt = getUnicodeDataAsSQLWCHAR(py_stmt, &isNewBuffer);

    rc = _python_IfxPy_do_prepare(conn_res->hdbc, stmt, stmt_size, stmt_res, options);
    if (isNewBuffer)
    {
        if (stmt) PyMem_Del(stmt);
    }

    if (rc < SQL_SUCCESS)
    {
        sprintf(error, "Statement Prepare Failed: %s", IFX_G(__python_stmt_err_msg));
        Py_XDECREF(py_stmt);
        return NULL;
    }
    Py_XDECREF(py_stmt);
    return (PyObject *)stmt_res;
}

/*!# IfxPy.prepare
*
* ===Description
* IBMDB_Statement IfxPy.prepare ( IFXConnection connection,
*                                  string statement [, array options] )
*
* IfxPy.prepare() creates a prepared SQL statement which can include 0 or
* more parameter markers (? characters) representing parameters for input,
* output, or input/output. You can pass parameters to the prepared statement
* using IfxPy.bind_param(), or for input values only, as an array passed to
* IfxPy.execute().
*
* There are three main advantages to using prepared statements in your
* application:
*        * Performance: when you prepare a statement, the database server
*         creates an optimized access plan for retrieving data with that
*         statement. Subsequently issuing the prepared statement with
*         IfxPy.execute() enables the statements to reuse that access plan
*         and avoids the overhead of dynamically creating a new access plan
*         for every statement you issue.
*        * Security: when you prepare a statement, you can include parameter
*         markers for input values. When you execute a prepared statement
*         with input values for placeholders, the database server checks each
*         input value to ensure that the type matches the column definition or
*         parameter definition.
*        * Advanced functionality: Parameter markers not only enable you to
*         pass input values to prepared SQL statements, they also enable you
*         to retrieve OUT and INOUT parameters from stored procedures using
*         IfxPy.bind_param().
*
* ===Parameters
* ====connection
*
*        A valid database connection resource variable as returned from
*        IfxPy.connect() 
*
* ====statement
*
*        An SQL statement, optionally containing one or more parameter markers.
*
* ====options
*
*        An dictionary containing statement options. You can use this parameter
*        to request a scrollable cursor on database servers that support this
*        functionality.
*
*        SQL_ATTR_CURSOR_TYPE
*             Passing the SQL_SCROLL_FORWARD_ONLY value requests a forward-only
*             cursor for this SQL statement. This is the default type of
*             cursor, and it is supported by all database servers. It is also
*             much faster than a scrollable cursor.
*             Passing the SQL_CURSOR_KEYSET_DRIVEN value requests a scrollable
*             cursor for this SQL statement. This type of cursor enables you
*             to fetch rows non-sequentially from the database server. However,
*             it is only supported by DB2 servers, and is much slower than
*             forward-only cursors.
*
* ===Return Values
* Returns a IFXStatement object if the SQL statement was successfully
* parsed and prepared by the database server. Returns FALSE if the database
* server returned an error. You can determine which error was returned by
* calling IfxPy.stmt_error() or IfxPy.stmt_errormsg().
*/
static PyObject *IfxPy_prepare(PyObject *self, PyObject *args)
{
    PyObject *py_conn_res = NULL;
    PyObject *options = NULL;
    conn_handle *conn_res;

    PyObject *py_stmt = NULL;

    if (!PyArg_ParseTuple(args, "OO|O", &py_conn_res, &py_stmt, &options))
        return NULL;

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }
        return _python_IfxPy_prepare_helper(conn_res, py_stmt, options);
    }

    return NULL;
}

static param_node* build_list(stmt_handle *stmt_res, int param_no, SQLSMALLINT data_type, SQLULEN precision, SQLSMALLINT scale, SQLSMALLINT nullable)
{
    param_node *tmp_curr = NULL, *curr = stmt_res->head_cache_list, *prev = NULL;

    /* Allocate memory and make new node to be added */
    tmp_curr = ALLOC(param_node);
    memset(tmp_curr, 0, sizeof(param_node));
    /* assign values */
    tmp_curr->data_type = data_type;
    tmp_curr->param_size = precision;
    tmp_curr->nullable = nullable;
    tmp_curr->scale = scale;
    tmp_curr->param_num = param_no;
    tmp_curr->file_options = SQL_FILE_READ;
    tmp_curr->param_type = SQL_PARAM_INPUT;

    while (curr != NULL)
    {
        prev = curr;
        curr = curr->next;
    }

    if (stmt_res->head_cache_list == NULL)
    {
        stmt_res->head_cache_list = tmp_curr;
    }
    else
    {
        prev->next = tmp_curr;
    }

    tmp_curr->next = curr;

    return tmp_curr;
}


static int _python_IfxPy_bind_data(stmt_handle *stmt_res, param_node *curr, PyObject *bind_data)
{
    int rc;
    SQLSMALLINT valueType = 0;
    SQLPOINTER    paramValuePtr;
#if  PY_MAJOR_VERSION < 3
    Py_ssize_t buffer_len = 0;
#endif
    SQLLEN param_length;

    /* Have to use SQLBindFileToParam if PARAM is type PARAM_FILE */
    /*** Need to fix this***/
    if (curr->param_type == PARAM_FILE)
    {
        PyObject *FileNameObj = NULL;
        /* Only string types can be bound */
        if (PyString_Check(bind_data))
        {
            if (PyUnicode_Check(bind_data))
            {
                FileNameObj = PyUnicode_AsASCIIString(bind_data);
                if (FileNameObj == NULL)
                {
                    return SQL_ERROR;
                }
            }
        }
        else
        {
            return SQL_ERROR;
        }
        curr->bind_indicator = 0;
        if (curr->svalue != NULL)
        {
            PyMem_Del(curr->svalue);
            curr->svalue = NULL;
        }
        if (FileNameObj != NULL)
        {
            curr->svalue = PyBytes_AsString(FileNameObj);
        }
        else
        {
            curr->svalue = PyBytes_AsString(bind_data);
        }
        curr->ivalue = strlen(curr->svalue);
        curr->svalue = memcpy(PyMem_Malloc((sizeof(char))*(curr->ivalue + 1)), curr->svalue, curr->ivalue);
        curr->svalue [curr->ivalue] = '\0';
        Py_XDECREF(FileNameObj);
        valueType = (SQLSMALLINT)curr->ivalue;
        /* Bind file name string */

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLBindFileToParam((SQLHSTMT)stmt_res->hstmt, curr->param_num,
                                curr->data_type, (SQLCHAR*)curr->svalue,
                                (SQLSMALLINT*)&(curr->ivalue), &(curr->file_options),
                                curr->ivalue, &(curr->bind_indicator));
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT,
                                            rc, 1, NULL, -1, 1);
        }
        return rc;
    }

    switch (TYPE(bind_data))
    {
    case PYTHON_FIXNUM:
        if (curr->data_type == SQL_BIGINT || curr->data_type == SQL_DECIMAL )
        {
            PyObject *tempobj = NULL;
#if  PY_MAJOR_VERSION >= 3
            PyObject *tempobj2 = NULL;
#endif
            tempobj = PyObject_Str(bind_data);
#if  PY_MAJOR_VERSION >= 3
            tempobj2 = PyUnicode_AsASCIIString(tempobj);
            Py_XDECREF(tempobj);
            tempobj = tempobj2;
#endif    
            curr->svalue = PyBytes_AsString(tempobj);
            curr->ivalue = strlen(curr->svalue);
            curr->svalue = memcpy(PyMem_Malloc((sizeof(char))*(curr->ivalue + 1)), curr->svalue, curr->ivalue);
            curr->svalue [curr->ivalue] = '\0';
            curr->bind_indicator = curr->ivalue;

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindParameter(stmt_res->hstmt, curr->param_num,
                                  curr->param_type, SQL_C_CHAR, curr->data_type,
                                  curr->param_size, curr->scale, curr->svalue, curr->param_size, NULL);
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT,
                                                rc, 1, NULL, -1, 1);
            }
            Py_XDECREF(tempobj);
        }
        else
        {
            curr->ivalue = (SQLINTEGER)PyLong_AsLong(bind_data);

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindParameter(stmt_res->hstmt, curr->param_num,
                                  curr->param_type, SQL_C_LONG, curr->data_type,
                                  curr->param_size, curr->scale, &curr->ivalue, 0, NULL);
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT,
                                                rc, 1, NULL, -1, 1);
            }
            curr->data_type = SQL_C_LONG;
        }
        break;

        /* Convert BOOLEAN types to LONG for DB2 / Cloudscape */
    case PYTHON_FALSE:
        curr->ivalue = 0;

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLBindParameter(stmt_res->hstmt, curr->param_num,
                              curr->param_type, SQL_C_LONG, curr->data_type, curr->param_size,
                              curr->scale, &curr->ivalue, 0, NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT,
                                            rc, 1, NULL, -1, 1);
        }
        curr->data_type = SQL_C_LONG;
        break;

    case PYTHON_TRUE:
        curr->ivalue = 1;

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLBindParameter(stmt_res->hstmt, curr->param_num,
                              curr->param_type, SQL_C_LONG, curr->data_type, curr->param_size,
                              curr->scale, &curr->ivalue, 0, NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT,
                                            rc, 1, NULL, -1, 1);
        }
        curr->data_type = SQL_C_LONG;
        break;

    case PYTHON_FLOAT:
        curr->fvalue = PyFloat_AsDouble(bind_data);
        Py_BEGIN_ALLOW_THREADS;
        rc = SQLBindParameter(stmt_res->hstmt, curr->param_num,
                              curr->param_type, SQL_C_DOUBLE, curr->data_type, curr->param_size,
                              curr->scale, &curr->fvalue, 0, NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT,
                                            rc, 1, NULL, -1, 1);
        }
        curr->data_type = SQL_C_DOUBLE;
        break;

    case PYTHON_UNICODE:
        {
            int isNewBuffer;
            if (PyObject_CheckBuffer(bind_data) && ( 
                curr->data_type == SQL_BINARY
		|| curr->data_type == SQL_VARBINARY
		|| curr->data_type == SQL_LONGVARBINARY || curr->data_type == SQL_INFX_RC_SET))
            {
#if  PY_MAJOR_VERSION >= 3
                Py_buffer tmp_buffer;
                PyObject_GetBuffer(bind_data, &tmp_buffer, PyBUF_SIMPLE);
                curr->uvalue = tmp_buffer.buf;
                curr->ivalue = tmp_buffer.len;
#else                    
                PyObject_AsReadBuffer(bind_data, (const void **) &(curr->uvalue), &buffer_len);
                curr->ivalue = buffer_len;
#endif
            }
	    
            else
            {
                if (curr->uvalue != NULL)
                {
                    PyMem_Del(curr->uvalue);
                    curr->uvalue = NULL;
                }
                curr->uvalue = getUnicodeDataAsSQLWCHAR(bind_data, &isNewBuffer);
#if PY_MAJOR_VERSION >=3 && PY_MINOR_VERSION >= 3
                curr->ivalue = PyUnicode_GetLength(bind_data);
#else
                curr->ivalue = PyUnicode_GetSize(bind_data);
#endif
                curr->ivalue = curr->ivalue * sizeof(SQLWCHAR);
            }
            param_length = curr->ivalue;
            if (curr->size != 0)
            {
                curr->ivalue = (curr->size + 1) * sizeof(SQLWCHAR);
            }

            if (curr->param_type == SQL_PARAM_OUTPUT || curr->param_type == SQL_PARAM_INPUT_OUTPUT)
            {
                if (curr->size == 0)
                {
                    if ( //(curr->data_type == SQL_BLOB) || (curr->data_type == SQL_CLOB) || 
                        (curr->data_type == SQL_BINARY)
                        || (curr->data_type == SQL_LONGVARBINARY)
                        || (curr->data_type == SQL_VARBINARY))
                    {
                        if (curr->ivalue <= (SQLLEN)curr->param_size)
                        {
                            curr->ivalue = curr->param_size + sizeof(SQLWCHAR);
                        }
                    }
                    else
                    {
                        if (curr->ivalue <= (SQLLEN)(curr->param_size * sizeof(SQLWCHAR)))
                        {
                            curr->ivalue = (curr->param_size + 1) * sizeof(SQLWCHAR);
                        }
                    }
                }
            }

            if (isNewBuffer == 0)
            {
		
                /* actually make a copy, since this will uvalue will be freed explicitly */
                SQLWCHAR* tmp = (SQLWCHAR*)ALLOC_N(SQLWCHAR, curr->ivalue + 1);
                memcpy(tmp, curr->uvalue, (param_length + sizeof(SQLWCHAR)));
                curr->uvalue = tmp;
            }
            else if (param_length <= (SQLLEN)curr->param_size)
            {
                SQLWCHAR* tmp = (SQLWCHAR*)ALLOC_N(SQLWCHAR, curr->ivalue + 1);
                memcpy(tmp, curr->uvalue, (param_length + sizeof(SQLWCHAR)));
                PyMem_Del(curr->uvalue);
                curr->uvalue = tmp;
            }
            switch (curr->data_type)
            {

            case SQL_BINARY:
            case SQL_LONGVARBINARY:
            case SQL_VARBINARY:
                /* account for bin_mode settings as well */
                curr->bind_indicator = param_length;
                valueType = SQL_C_BINARY;
                paramValuePtr = (SQLPOINTER)curr->uvalue;
                break;

            case SQL_TYPE_TIMESTAMP:
                valueType = SQL_C_WCHAR;
                if (param_length == 0)
                {
                    curr->bind_indicator = SQL_NULL_DATA;
                }
                else
                {
                    curr->bind_indicator = SQL_NTS;
                }
                if (curr->uvalue [10] == 'T')
                {
                    curr->uvalue [10] = ' ';
                }
                paramValuePtr = (SQLPOINTER)(curr->uvalue);
                break;

	    case SQL_INFX_RC_SET:
	    case SQL_INFX_RC_MULTISET:
            case SQL_INFX_RC_LIST:
            case SQL_INFX_RC_ROW:
            case SQL_INFX_RC_COLLECTION:
	    case SQL_INFX_UDT_FIXED:
            case SQL_INFX_UDT_VARYING:
     	        if (curr->param_type == SQL_PARAM_OUTPUT ||curr->param_type == SQL_PARAM_INPUT_OUTPUT) {
                            curr->bind_indicator = param_length;
                            paramValuePtr = (SQLPOINTER)curr;
                        } else {
                            curr->bind_indicator = SQL_DATA_AT_EXEC;
                            paramValuePtr = (SQLPOINTER)(curr);
                        }
                        valueType = SQL_C_BINARY; 
                        break;

            default:
                valueType = SQL_C_WCHAR;
                curr->bind_indicator = param_length;
                paramValuePtr = (SQLPOINTER)(curr->uvalue);
            }

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindParameter(stmt_res->hstmt,
                                  curr->param_num,
                                  curr->param_type,
                                  valueType,
                                  curr->data_type,
                                  curr->param_size,
                                  curr->scale,
                                  paramValuePtr,
                                  curr->ivalue,
                                  &(curr->bind_indicator));

            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
            }
            curr->data_type = valueType;
        }
        break;

    case PYTHON_STRING:
        {
            char* tmp;
            if (PyObject_CheckBuffer(bind_data) && (
                curr->data_type == SQL_BINARY
                || curr->data_type == SQL_VARBINARY
                || curr->data_type == SQL_LONGVARBINARY))
            {
#if  PY_MAJOR_VERSION >= 3
                Py_buffer tmp_buffer;
                PyObject_GetBuffer(bind_data, &tmp_buffer, PyBUF_SIMPLE);
                curr->svalue = tmp_buffer.buf;
                curr->ivalue = tmp_buffer.len;
#else
                PyObject_AsReadBuffer(bind_data, (const void **) &(curr->svalue), &buffer_len);
                curr->ivalue = buffer_len;
#endif
            }
            else
            {
                if (curr->svalue != NULL)
                {
                    PyMem_Del(curr->svalue);
                    curr->svalue = NULL;
                }
                curr->svalue = PyBytes_AsString(bind_data);   // It is PyString_AsString() in PY_MAJOR_VERSION<3, and code execution will not come here in PY_MAJOR_VERSION>=3 
                curr->ivalue = strlen(curr->svalue);
            }
            param_length = curr->ivalue;

            //An extra parameter is given by the client to pick the size of the
            //string returned. The string is then truncate past that size.
            //If no size is given then use BUFSIZ to return the string.

            if (curr->size != 0)
            {
                curr->ivalue = curr->size;
            }

            if (curr->param_type == SQL_PARAM_OUTPUT || curr->param_type == SQL_PARAM_INPUT_OUTPUT)
            {
                if (curr->size == 0)
                {
                    if (curr->ivalue <= (SQLLEN)curr->param_size)
                    {
                        curr->ivalue = curr->param_size + 1;
                    }
                }
            }
            tmp = ALLOC_N(char, curr->ivalue + 1);
            curr->svalue = memcpy(tmp, curr->svalue, param_length);
            curr->svalue [param_length] = '\0';

            switch (curr->data_type)
            {
            case SQL_BINARY:
            case SQL_LONGVARBINARY:
            case SQL_VARBINARY:
                /* account for bin_mode settings as well */
                curr->bind_indicator = curr->ivalue;
                if (curr->param_type == SQL_PARAM_OUTPUT || curr->param_type == SQL_PARAM_INPUT_OUTPUT)
                {
                    curr->ivalue = curr->ivalue - 1;
                    curr->bind_indicator = param_length;
                }

                valueType = SQL_C_BINARY;
                paramValuePtr = (SQLPOINTER)curr->svalue;
                break;

                // This option should handle most other types such as DATE, VARCHAR etc
            case SQL_TYPE_TIMESTAMP:
                valueType = SQL_C_CHAR;
                curr->bind_indicator = curr->ivalue;
                if (curr->param_type == SQL_PARAM_OUTPUT || curr->param_type == SQL_PARAM_INPUT_OUTPUT)
                {
                    if (param_length == 0)
                    {
                        curr->bind_indicator = SQL_NULL_DATA;
                    }
                    else
                    {
                        curr->bind_indicator = SQL_NTS;
                    }
                }
                if (curr->svalue [10] == 'T')
                {
                    curr->svalue [10] = ' ';
                }
                paramValuePtr = (SQLPOINTER)(curr->svalue);
                break;

  	   case SQL_INFX_RC_SET:
           case SQL_INFX_RC_MULTISET:
	   case SQL_INFX_RC_ROW:
	   case SQL_INFX_RC_LIST:
	   case SQL_INFX_RC_COLLECTION:
	   case SQL_INFX_UDT_FIXED:
           case SQL_INFX_UDT_VARYING:
		        if (curr->param_type == SQL_PARAM_OUTPUT ||curr->param_type == SQL_PARAM_INPUT_OUTPUT) {
                            curr->bind_indicator = param_length;
                            paramValuePtr = (SQLPOINTER)curr;
                        } else {
                            curr->bind_indicator = SQL_DATA_AT_EXEC;
                            paramValuePtr = (SQLPOINTER)(curr);
                        }
                        valueType = SQL_C_BINARY;
                        break;

            default:
                valueType = SQL_C_CHAR;
                curr->bind_indicator = curr->ivalue;
                if (curr->param_type == SQL_PARAM_OUTPUT || curr->param_type == SQL_PARAM_INPUT_OUTPUT)
                {
                    curr->bind_indicator = SQL_NTS;
                }
                paramValuePtr = (SQLPOINTER)(curr->svalue);
            }

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindParameter(stmt_res->hstmt, curr->param_num,
                                  curr->param_type, valueType, curr->data_type, curr->param_size,
                                  curr->scale, paramValuePtr, curr->ivalue, &(curr->bind_indicator));
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT,
                                                rc, 1, NULL, -1, 1);
            }
            curr->data_type = valueType;
        }
        break;

    case PYTHON_DECIMAL:
        if (curr->data_type == SQL_DECIMAL)
        {
            PyObject *tempobj = NULL;
#if  PY_MAJOR_VERSION >= 3
            PyObject *tempobj2 = NULL;
#endif
            if (curr->svalue != NULL)
            {
                PyMem_Del(curr->svalue);
                curr->svalue = NULL;
            }
            tempobj = PyObject_Str(bind_data);
#if PY_MAJOR_VERSION >= 3
            tempobj2 = PyUnicode_AsASCIIString(tempobj);
            Py_XDECREF(tempobj);
            tempobj = tempobj2;
#endif
            curr->svalue = PyBytes_AsString(tempobj);
            curr->ivalue = strlen(curr->svalue);
            curr->svalue = estrdup(curr->svalue);
            curr->svalue [curr->ivalue] = '\0';
            valueType = SQL_C_CHAR;
            paramValuePtr = (SQLPOINTER)(curr->svalue);
            curr->bind_indicator = curr->ivalue;

            Py_BEGIN_ALLOW_THREADS;
            rc = SQLBindParameter(stmt_res->hstmt, curr->param_num, curr->param_type, valueType, curr->data_type, curr->param_size, curr->scale, paramValuePtr, curr->ivalue, &(curr->bind_indicator));
            Py_END_ALLOW_THREADS;

            if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
            }
            curr->data_type = valueType;
            Py_XDECREF(tempobj);
            break;
        }


    case PYTHON_DATE:
        curr->date_value = ALLOC(DATE_STRUCT);
        curr->date_value->year = PyDateTime_GET_YEAR(bind_data);
        curr->date_value->month = PyDateTime_GET_MONTH(bind_data);
        curr->date_value->day = PyDateTime_GET_DAY(bind_data);

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLBindParameter(stmt_res->hstmt, curr->param_num,
                              curr->param_type, SQL_C_TYPE_DATE, curr->data_type, curr->param_size,
                              curr->scale, curr->date_value, curr->ivalue, &(curr->bind_indicator));
        Py_END_ALLOW_THREADS;
        break;

    case PYTHON_TIME:
        curr->time_value = ALLOC(TIME_STRUCT);
        curr->time_value->hour = PyDateTime_TIME_GET_HOUR(bind_data);
        curr->time_value->minute = PyDateTime_TIME_GET_MINUTE(bind_data);
        curr->time_value->second = PyDateTime_TIME_GET_SECOND(bind_data);

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLBindParameter(stmt_res->hstmt, curr->param_num,
                              curr->param_type, SQL_C_TYPE_TIME, curr->data_type, curr->param_size,
                              curr->scale, curr->time_value, curr->ivalue, &(curr->bind_indicator));
        Py_END_ALLOW_THREADS;
        break;

    case PYTHON_TIMESTAMP:
        curr->ts_value = ALLOC(TIMESTAMP_STRUCT);
        curr->ts_value->year = PyDateTime_GET_YEAR(bind_data);
        curr->ts_value->month = PyDateTime_GET_MONTH(bind_data);
        curr->ts_value->day = PyDateTime_GET_DAY(bind_data);
        curr->ts_value->hour = PyDateTime_DATE_GET_HOUR(bind_data);
        curr->ts_value->minute = PyDateTime_DATE_GET_MINUTE(bind_data);
        curr->ts_value->second = PyDateTime_DATE_GET_SECOND(bind_data);
        curr->ts_value->fraction = PyDateTime_DATE_GET_MICROSECOND(bind_data) * 1000;

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLBindParameter(stmt_res->hstmt, curr->param_num,
                              curr->param_type, SQL_C_TYPE_TIMESTAMP, curr->data_type, curr->param_size,
                              curr->scale, curr->ts_value, curr->ivalue, &(curr->bind_indicator));
        Py_END_ALLOW_THREADS;
        break;

    case PYTHON_TIMEDELTA:
        curr->interval_value = ALLOC(SQL_INTERVAL_STRUCT);
        curr->interval_value->interval_type = SQL_IS_DAY_TO_SECOND;
        if (((PyDateTime_Delta*)bind_data)->days < 0) {
          curr->interval_value->interval_sign = 0;
          curr->interval_value->intval.day_second.day = (((PyDateTime_Delta*)bind_data)->days * -1) - 1;
          curr->interval_value->intval.day_second.second = 86400 - (((PyDateTime_Delta*)bind_data)->seconds);
        } else {
          curr->interval_value->interval_sign = 1;
          curr->interval_value->intval.day_second.day = ((PyDateTime_Delta*)bind_data)->days;
          curr->interval_value->intval.day_second.second = (((PyDateTime_Delta*)bind_data)->seconds);
        }
        curr->interval_value->intval.day_second.hour = curr->interval_value->intval.day_second.second / 3600;
        curr->interval_value->intval.day_second.second -= curr->interval_value->intval.day_second.hour * 3600;
        curr->interval_value->intval.day_second.minute = curr->interval_value->intval.day_second.second / 60;
        curr->interval_value->intval.day_second.second -= curr->interval_value->intval.day_second.minute * 60;
        curr->interval_value->intval.day_second.fraction = 0;

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLBindParameter(stmt_res->hstmt, curr->param_num,
                              curr->param_type, SQL_C_INTERVAL_DAY_TO_SECOND, curr->data_type, curr->param_size,
                              curr->scale, curr->interval_value, curr->ivalue, &(curr->bind_indicator));
        Py_END_ALLOW_THREADS;
        break;

    case PYTHON_NIL:
        curr->ivalue = SQL_NULL_DATA;

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLBindParameter(stmt_res->hstmt, curr->param_num,
                              curr->param_type, SQL_C_DEFAULT, curr->data_type, curr->param_size,
                              curr->scale, &curr->ivalue, 0, (SQLLEN *)&(curr->ivalue));
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT,
                                            rc, 1, NULL, -1, 1);
        }
        break;

    default:
        return SQL_ERROR;
    }
    return rc;
}

static int _python_IfxPy_execute_helper2(stmt_handle *stmt_res, PyObject *data, int bind_cmp_list, int bind_params)
{
    int rc = SQL_SUCCESS;
    param_node *curr = NULL;    /* To traverse the list */
    PyObject *bind_data;         /* Data value from symbol table */
    char error [DB_MAX_ERR_MSG_LEN];

    /* Used in call to SQLDescribeParam if needed */
    SQLSMALLINT param_no;
    SQLSMALLINT data_type;
    SQLULEN     precision;
    SQLSMALLINT scale;
    SQLSMALLINT nullable;

    /* This variable means that we bind the complete list of params cached */
    /* The values used are fetched from the active symbol table */
    /* TODO: Enhance this part to check for stmt_res->file_param */
    /* If this flag is set, then use SQLBindParam, else use SQLExtendedBind */
    if (bind_cmp_list)
    {
        /* Bind the complete list sequentially */
        /* Used when no parameters array is passed in */
        curr = stmt_res->head_cache_list;

        while (curr != NULL)
        {
            /* Fetch data from symbol table */
            if (curr->param_type == PARAM_FILE)
                bind_data = curr->var_pyvalue;
            else
            {
                bind_data = curr->var_pyvalue;
            }
            if (bind_data == NULL)
                return -1;

            rc = _python_IfxPy_bind_data(stmt_res, curr, bind_data);
            if (rc == SQL_ERROR)
            {
                sprintf(error, "Binding Error 1: %s",
                        IFX_G(__python_stmt_err_msg));
                PyErr_SetString(PyExc_Exception, error);
                return rc;
            }
            curr = curr->next;
        }
        return 0;
    }
    else
    {
        /* Bind only the data value passed in to the Current Node */
        if (data != NULL)
        {
            if (bind_params)
            {
                //This condition applies if the parameter has not been
                //bound using IfxPy.bind_param. Need to describe the
                //parameter and then bind it.

                param_no = ++stmt_res->num_params;

                Py_BEGIN_ALLOW_THREADS;
                rc = SQLDescribeParam((SQLHSTMT)stmt_res->hstmt, param_no,
                    (SQLSMALLINT*)&data_type, &precision, (SQLSMALLINT*)&scale,
                                      (SQLSMALLINT*)&nullable);
                Py_END_ALLOW_THREADS;

                if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
                {
                    _python_IfxPy_check_sql_errors(stmt_res->hstmt,
                                                    SQL_HANDLE_STMT,
                                                    rc, 1, NULL, -1, 1);
                }
                if (rc == SQL_ERROR)
                {
                    sprintf(error, "Describe Param Failed: %s",
                            IFX_G(__python_stmt_err_msg));
                    PyErr_SetString(PyExc_Exception, error);
                    return rc;
                }
                curr = build_list(stmt_res, param_no, data_type, precision,
                                  scale, nullable);
                rc = _python_IfxPy_bind_data(stmt_res, curr, data);
                if (rc == SQL_ERROR)
                {
                    sprintf(error, "Binding Error 2: %s",
                            IFX_G(__python_stmt_err_msg));
                    PyErr_SetString(PyExc_Exception, error);
                    return rc;
                }
            }
            else
            {
                //This is always at least the head_cache_node -- assigned in
                //IfxPy.execute(), if params have been bound.

                curr = stmt_res->current_node;
                if (curr != NULL)
                {
                    rc = _python_IfxPy_bind_data(stmt_res, curr, data);
                    if (rc == SQL_ERROR)
                    {
                        sprintf(error, "Binding Error 2: %s",
                                IFX_G(__python_stmt_err_msg));
                        PyErr_SetString(PyExc_Exception, error);
                        return rc;
                    }
                    stmt_res->current_node = curr->next;
                }
            }
            return rc;
        }
    }
    return rc;
}

static PyObject *_python_IfxPy_execute_helper1(stmt_handle *stmt_res, PyObject *parameters_tuple)
{
    Py_ssize_t numOpts = 0;
    int rc, i, bind_params = 0;
    SQLSMALLINT num;
    SQLPOINTER valuePtr;
    PyObject *data;
    char error [DB_MAX_ERR_MSG_LEN];

    Py_BEGIN_ALLOW_THREADS;
    // Free any cursors that might have been allocated in a previous call to SQLExecute
    SQLFreeStmt((SQLHSTMT)stmt_res->hstmt, SQL_CLOSE);
    Py_END_ALLOW_THREADS;

    // This ensures that each call to IfxPy.execute start from scratch 
    stmt_res->current_node = stmt_res->head_cache_list;

    Py_BEGIN_ALLOW_THREADS;
    rc = SQLNumParams((SQLHSTMT)stmt_res->hstmt, (SQLSMALLINT*)&num);
    Py_END_ALLOW_THREADS;

    if (num != 0)
    {
        // Parameter Handling 
        if (!NIL_P(parameters_tuple))
        {
            // Make sure IfxPy.bind_param has been called 
            // If the param list is NULL -- ERROR 
            if (stmt_res->head_cache_list == NULL)
            {
                bind_params = 1;
            }

            if (!PyTuple_Check(parameters_tuple))
            {
                PyErr_SetString(PyExc_Exception, "Param is not a tuple");
                return NULL;
            }

            numOpts = PyTuple_Size(parameters_tuple);

            if (numOpts > num)
            {
                // More are passed in -- Warning - Use the max number present
                // The z portion is a length specifier which says the argument will be size_t in length
                // https://en.wikipedia.org/wiki/Printf_format_string#printf_format_placeholders
                sprintf(error, "%zu params bound not matching %d required",
                        numOpts, num);
                PyErr_SetString(PyExc_Exception, error);
                numOpts = stmt_res->num_params;
            }
            else if (numOpts < num)
            {
                // If there are less params passed in, than are present 
                // -- Error
                sprintf(error, "%zu params bound not matching %d required",
                        numOpts, num);
                PyErr_SetString(PyExc_Exception, error);
                return NULL;
            }

            for (i = 0; i < numOpts; i++)
            {
                // Bind values from the parameters_tuple to params 
                data = PyTuple_GetItem(parameters_tuple, i);

                // The 0 denotes that you work only with the current node.
                //The 4th argument specifies whether the data passed in
                //has been described. So we need to call SQLDescribeParam
                //before binding depending on this.

                rc = _python_IfxPy_execute_helper2(stmt_res, data, 0, bind_params);
                if (rc == SQL_ERROR)
                {
                    sprintf(error, "Binding Error: %s", IFX_G(__python_stmt_err_msg));
                    PyErr_SetString(PyExc_Exception, error);
                    return NULL;
                }
            }
        }
        else
        {
            // No additional params passed in. Use values already bound. 
            if (num > stmt_res->num_params)
            {
                // More parameters than we expected 
                sprintf(error, "%d params bound not matching %d required",
                        stmt_res->num_params, num);
                PyErr_SetString(PyExc_Exception, error);
            }
            else if (num < stmt_res->num_params)
            {
                // Fewer parameters than we expected 
                sprintf(error, "%d params bound not matching %d required",
                        stmt_res->num_params, num);
                PyErr_SetString(PyExc_Exception, error);
                return NULL;
            }

            // Param cache node list is empty -- No params bound 
            if (stmt_res->head_cache_list == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Parameters not bound");
                return NULL;
            }
            else
            {
                //The 1 denotes that you work with the whole list
                //And bind sequentially

                rc = _python_IfxPy_execute_helper2(stmt_res, NULL, 1, 0);
                if (rc == SQL_ERROR)
                {
                    sprintf(error, "Binding Error 3: %s", IFX_G(__python_stmt_err_msg));
                    PyErr_SetString(PyExc_Exception, error);
                    return NULL;
                }
            }
        }
    }
    else
    {
        //No Parameters
        //We just execute the statement. No additional work needed.

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLExecute((SQLHSTMT)stmt_res->hstmt);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt,
                                            SQL_HANDLE_STMT,
                                            rc, 1, NULL, -1, 1);
        }
        if (rc == SQL_ERROR)
        {
            sprintf(error, "Statement Execute Failed: %s", IFX_G(__python_stmt_err_msg));
            PyErr_SetString(PyExc_Exception, error);
            return NULL;
        }
        Py_INCREF(Py_True);
        return Py_True;
    }

    /* Execute Stmt -- All parameters bound */
    Py_BEGIN_ALLOW_THREADS;
    rc = SQLExecute((SQLHSTMT)stmt_res->hstmt);
    Py_END_ALLOW_THREADS;
    if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
    {
        _python_IfxPy_check_sql_errors(stmt_res->hstmt,
                                        SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
    }
    if (rc == SQL_ERROR)
    {
        sprintf(error, "Statement Execute Failed: %s", IFX_G(__python_stmt_err_msg));
        PyErr_SetString(PyExc_Exception, error);
        return NULL;
    }
    if (rc == SQL_NEED_DATA)
    {
        rc = SQLParamData((SQLHSTMT)stmt_res->hstmt, (SQLPOINTER *)&valuePtr);
        while (rc == SQL_NEED_DATA)
        {
            /* passing data value for a parameter */
            if (!NIL_P(((param_node*)valuePtr)->svalue))
            {
                Py_BEGIN_ALLOW_THREADS;
                rc = SQLPutData((SQLHSTMT)stmt_res->hstmt, (SQLPOINTER)(((param_node*)valuePtr)->svalue), ((param_node*)valuePtr)->ivalue);
                Py_END_ALLOW_THREADS;
            }
            else
            {
                Py_BEGIN_ALLOW_THREADS;
                rc = SQLPutData((SQLHSTMT)stmt_res->hstmt, (SQLPOINTER)(((param_node*)valuePtr)->uvalue), ((param_node*)valuePtr)->ivalue);
                Py_END_ALLOW_THREADS;
            }

            if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT,
                                                rc, 1, NULL, -1, 1);
            }
            if (rc == SQL_ERROR)
            {
                sprintf(error, "Sending data failed: %s",
                        IFX_G(__python_stmt_err_msg));
                PyErr_SetString(PyExc_Exception, error);
                return NULL;
            }

            rc = SQLParamData((SQLHSTMT)stmt_res->hstmt, (SQLPOINTER *)&valuePtr);
        }

        if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt,
                                            SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
        }
        if (rc == SQL_ERROR)
        {
            sprintf(error, "Sending data failed: %s",
                    IFX_G(__python_stmt_err_msg));
            PyErr_SetString(PyExc_Exception, error);
            return NULL;
        }
    }

    /* cleanup dynamic bindings if present */
    if (bind_params == 1)
    {
        _python_IfxPy_clear_param_cache(stmt_res);
    }

    if (rc != SQL_ERROR)
    {
        Py_INCREF(Py_True);
        return Py_True;
    }
    return NULL;
}

/*!# IfxPy.execute
*
* ===Description
* Py_True/Py_False IfxPy.execute ( IFXStatement stmt [, tuple parameters] )
*
* IfxPy.execute() executes an SQL statement that was prepared by
* IfxPy.prepare().
*
* If the SQL statement returns a result set, for example, a SELECT statement
* or a CALL to a stored procedure that returns one or more result sets, you
* can retrieve a row as an tuple/dictionary from the stmt resource using
* IfxPy.fetch_assoc(), IfxPy.fetch_both(), or IfxPy.fetch_tuple().
* Alternatively, you can use IfxPy.fetch_row() to move the result set pointer
* to the next row and fetch a column at a time from that row with
* IfxPy.result().
*
* Refer to IfxPy.prepare() for a brief discussion of the advantages of using
* IfxPy.prepare() and IfxPy.execute() rather than IfxPy.exec().
*
* ===Parameters
* ====stmt
*
*        A prepared statement returned from IfxPy.prepare().
*
* ====parameters
*
*        An tuple of input parameters matching any parameter markers contained
* in the prepared statement.
*
* ===Return Values
*
* Returns Py_True on success or Py_False on failure.
*/
static PyObject *IfxPy_execute(PyObject *self, PyObject *args)
{
    PyObject *py_stmt_res = NULL;
    PyObject *parameters_tuple = NULL;
    stmt_handle *stmt_res;
    if (!PyArg_ParseTuple(args, "O|O", &py_stmt_res, &parameters_tuple))
        return NULL;

    if (!NIL_P(py_stmt_res))
    {
        if (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
            return NULL;
        }
        else
        {
            stmt_res = (stmt_handle *)py_stmt_res;
        }
        return _python_IfxPy_execute_helper1(stmt_res, parameters_tuple);
    }
    else
    {
        PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
        return NULL;
    }

}


/*!# IfxPy.conn_errormsg
*
* ===Description
* string IfxPy.conn_errormsg ( [resource connection] )
*
* IfxPy.conn_errormsg() returns an error message and SQLCODE value
* representing the reason the last database connection attempt failed.
* As IfxPy.connect() returns FALSE in the event of a failed connection
* attempt, do not pass any parameters to IfxPy.conn_errormsg() to retrieve
* the associated error message and SQLCODE value.
*
* If, however, the connection was successful but becomes invalid over time,
* you can pass the connection parameter to retrieve the associated error
* message and SQLCODE value for a specific connection.
* ===Parameters
*
* ====connection
*        A connection resource associated with a connection that initially
* succeeded, but which over time became invalid.
*
* ===Return Values
*
* Returns a string containing the error message and SQLCODE value resulting
* from a failed connection attempt. If there is no error associated with the
* last connection attempt, IfxPy.conn_errormsg() returns an empty string.
*/
static PyObject *IfxPy_conn_errormsg(PyObject *self, PyObject *args)
{
    conn_handle *conn_res = NULL;
    PyObject *py_conn_res = NULL;
    PyObject *retVal = NULL;
    char* return_str = NULL;    // This variable is used by _python_IfxPy_check_sql_errors to return err  strings

    if (!PyArg_ParseTuple(args, "|O", &py_conn_res))
        return NULL;

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }

        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
        }

        return_str = ALLOC_N(char, DB_MAX_ERR_MSG_LEN);

        memset(return_str, 0, DB_MAX_ERR_MSG_LEN);

        _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, -1, 0,
                                        return_str, IDS_ERRMSG,
                                        conn_res->errormsg_recno_tracker);
        if (conn_res->errormsg_recno_tracker - conn_res->error_recno_tracker >= 1)
            conn_res->error_recno_tracker = conn_res->errormsg_recno_tracker;
        conn_res->errormsg_recno_tracker++;

        retVal = StringOBJ_FromASCII(return_str);
        if (return_str != NULL)
        {
            PyMem_Del(return_str);
            return_str = NULL;
        }
        return retVal;
    }
    else
    {
        return StringOBJ_FromASCII(IFX_G(__python_conn_err_msg));
    }
}
/*!# IfxPy_conn_warn
*
* ===Description
* string IfxPy.conn_warn ( [resource connection] )
*
* IfxPy.conn_warn() returns a warning message and SQLCODE value
* representing the reason the last database connection attempt failed.
* As IfxPy.connect() returns FALSE in the event of a failed connection
* attempt, do not pass any parameters to IfxPy.warn_conn_msg() to retrieve
* the associated warning message and SQLCODE value.
*
* If, however, the connection was successful but becomes invalid over time,
* you can pass the connection parameter to retrieve the associated warning
* message and SQLCODE value for a specific connection.
* ===Parameters
*
* ====connection
*      A connection resource associated with a connection that initially
* succeeded, but which over time became invalid.
*
* ===Return Values
*
* Returns a string containing the warning message and SQLCODE value resulting
* from a failed connection attempt. If there is no warning associated with the
* last connection attempt, IfxPy.warn_conn_msg() returns an empty string.
*/
static PyObject *IfxPy_conn_warn(PyObject *self, PyObject *args)
{
    conn_handle *conn_res = NULL;
    PyObject *py_conn_res = NULL;
    PyObject *retVal = NULL;
    char *return_str = NULL; // This variable is used by _python_IfxPy_check_sql_errors to return warning strings

    if (!PyArg_ParseTuple(args, "|O", &py_conn_res))
        return NULL;

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }
        return_str = ALLOC_N(char, DB_MAX_ERR_MSG_LEN);
        PyErr_Clear();
        memset(return_str, 0, SQL_SQLSTATE_SIZE + 1);

        _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, 1, 0,
                                        return_str, IDS_WARNMSG,
                                        conn_res->error_recno_tracker);
        if (conn_res->error_recno_tracker - conn_res->errormsg_recno_tracker >= 1)
        {
            conn_res->errormsg_recno_tracker = conn_res->error_recno_tracker;
        }
        conn_res->error_recno_tracker++;

        retVal = StringOBJ_FromASCII(return_str);
        if (return_str != NULL)
        {
            PyMem_Del(return_str);
            return_str = NULL;
        }
        return retVal;
    }
    else
    {
        return StringOBJ_FromASCII(IFX_G(__python_conn_warn_msg));
    }
}

/*!# IfxPy.stmt_warn
*
* ===Description
* string IfxPy.stmt_warn ( [resource stmt] )
*
* Returns a string containing the last SQL statement error message.
*
* If you do not pass a statement resource as an argument to
* IfxPy.warn_stmt_msg(), the driver returns the warning message associated with
* the last attempt to return a statement resource, for example, from
* IfxPy.prepare() or IfxPy.exec().
*
* ===Parameters
*
* ====stmt
*      A valid statement resource.
*
* ===Return Values
*
* Returns a string containing the warning message and SQLCODE value for the last
* warning that occurred issuing an SQL statement.
*/
static PyObject *IfxPy_stmt_warn(PyObject *self, PyObject *args)
{
    stmt_handle *stmt_res = NULL;
    PyObject *py_stmt_res = NULL;
    PyObject *retVal = NULL;
    char* return_str = NULL;    // This variable is used by _python_IfxPy_check_sql_errors to return err strings


    if (!PyArg_ParseTuple(args, "|O", &py_stmt_res))
        return NULL;

    if (!NIL_P(py_stmt_res))
    {
        if (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
            return NULL;
        }
        else
        {
            stmt_res = (stmt_handle *)py_stmt_res;
        }
        return_str = ALLOC_N(char, DB_MAX_ERR_MSG_LEN);

        memset(return_str, 0, DB_MAX_ERR_MSG_LEN);

        _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, 1, 0,
                                        return_str, IDS_WARNMSG,
                                        stmt_res->errormsg_recno_tracker);
        if (stmt_res->errormsg_recno_tracker - stmt_res->error_recno_tracker >= 1)
            stmt_res->error_recno_tracker = stmt_res->errormsg_recno_tracker;
        stmt_res->errormsg_recno_tracker++;

        retVal = StringOBJ_FromASCII(return_str);
        if (return_str != NULL)
        {
            PyMem_Del(return_str);
            return_str = NULL;
        }
        return retVal;
    }
    else
    {
        return StringOBJ_FromASCII(IFX_G(__python_stmt_warn_msg));
    }
}

/*!# IfxPy.stmt_errormsg
*
* ===Description
* string IfxPy.stmt_errormsg ( [resource stmt] )
*
* Returns a string containing the last SQL statement error message.
*
* If you do not pass a statement resource as an argument to
* IfxPy.stmt_errormsg(), the driver returns the error message associated with
* the last attempt to return a statement resource, for example, from
* IfxPy.prepare() or IfxPy.exec().
*
* ===Parameters
*
* ====stmt
*        A valid statement resource.
*
* ===Return Values
*
* Returns a string containing the error message and SQLCODE value for the last
* error that occurred issuing an SQL statement.
*/
static PyObject *IfxPy_stmt_errormsg(PyObject *self, PyObject *args)
{
    stmt_handle *stmt_res = NULL;
    PyObject *py_stmt_res = NULL;
    PyObject *retVal = NULL;
    char* return_str = NULL;    // This variable is used by _python_IfxPy_check_sql_errors to return err strings


    if (!PyArg_ParseTuple(args, "|O", &py_stmt_res))
        return NULL;

    if (!NIL_P(py_stmt_res))
    {
        if (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
            return NULL;
        }
        else
        {
            stmt_res = (stmt_handle *)py_stmt_res;
        }
        return_str = ALLOC_N(char, DB_MAX_ERR_MSG_LEN);

        memset(return_str, 0, DB_MAX_ERR_MSG_LEN);

        _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, -1, 0,
                                        return_str, IDS_ERRMSG,
                                        stmt_res->errormsg_recno_tracker);
        if (stmt_res->errormsg_recno_tracker - stmt_res->error_recno_tracker >= 1)
            stmt_res->error_recno_tracker = stmt_res->errormsg_recno_tracker;
        stmt_res->errormsg_recno_tracker++;

        retVal = StringOBJ_FromASCII(return_str);
        if (return_str != NULL)
        {
            PyMem_Del(return_str);
            return_str = NULL;
        }
        return retVal;
    }
    else
    {
        return StringOBJ_FromASCII(IFX_G(__python_stmt_err_msg));
    }
}

/*!# IfxPy.conn_error
* ===Description
* string IfxPy.conn_error ( [resource connection] )
*
* IfxPy.conn_error() returns an SQLSTATE value representing the reason the
* last attempt to connect to a database failed. As IfxPy.connect() returns
* FALSE in the event of a failed connection attempt, you do not pass any
* parameters to IfxPy.conn_error() to retrieve the SQLSTATE value.
*
* If, however, the connection was successful but becomes invalid over time, you
* can pass the connection parameter to retrieve the SQLSTATE value for a
* specific connection.
*
* To learn what the SQLSTATE value means, you can issue the following command
* at a DB2 Command Line Processor prompt: db2 '? sqlstate-value'. You can also
* call IfxPy.conn_errormsg() to retrieve an explicit error message and the
* associated SQLCODE value.
*
* ===Parameters
*
* ====connection
*        A connection resource associated with a connection that initially
* succeeded, but which over time became invalid.
*
* ===Return Values
*
* Returns the SQLSTATE value resulting from a failed connection attempt.
* Returns an empty string if there is no error associated with the last
* connection attempt.
*/
static PyObject *IfxPy_conn_error(PyObject *self, PyObject *args)
{
    conn_handle *conn_res = NULL;
    PyObject *py_conn_res = NULL;
    PyObject *retVal = NULL;
    char *return_str = NULL; // This variable is used by _python_IfxPy_check_sql_errors to return err strings 

    if (!PyArg_ParseTuple(args, "|O", &py_conn_res))
        return NULL;

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }
        return_str = ALLOC_N(char, SQL_SQLSTATE_SIZE + 1);

        memset(return_str, 0, SQL_SQLSTATE_SIZE + 1);

        _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, -1, 0,
                                        return_str, IDS_ERR,
                                        conn_res->error_recno_tracker);
        if (conn_res->error_recno_tracker - conn_res->errormsg_recno_tracker >= 1)
        {
            conn_res->errormsg_recno_tracker = conn_res->error_recno_tracker;
        }
        conn_res->error_recno_tracker++;

        retVal = StringOBJ_FromASCII(return_str);
        if (return_str != NULL)
        {
            PyMem_Del(return_str);
            return_str = NULL;
        }
        return retVal;
    }
    else
    {
        return StringOBJ_FromASCII(IFX_G(__python_conn_err_state));
    }
}

/*!# IfxPy.stmt_error
*
* ===Description
* string IfxPy.stmt_error ( [resource stmt] )
*
* Returns a string containing the SQLSTATE value returned by an SQL statement.
*
* If you do not pass a statement resource as an argument to
* IfxPy.stmt_error(), the driver returns the SQLSTATE value associated with
* the last attempt to return a statement resource, for example, from
* IfxPy.prepare() or IfxPy.exec().
*
* To learn what the SQLSTATE value means, you can issue the following command
* at a DB2 Command Line Processor prompt: db2 '? sqlstate-value'. You can also
* call IfxPy.stmt_errormsg() to retrieve an explicit error message and the
* associated SQLCODE value.
*
* ===Parameters
*
* ====stmt
*        A valid statement resource.
*
* ===Return Values
*
* Returns a string containing an SQLSTATE value.
*/
static PyObject *IfxPy_stmt_error(PyObject *self, PyObject *args)
{
    stmt_handle *stmt_res = NULL;
    PyObject *py_stmt_res = NULL;
    PyObject *retVal = NULL;
    char* return_str = NULL; // This variable is used by _python_IfxPy_check_sql_errors to return err strings 

    if (!PyArg_ParseTuple(args, "|O", &py_stmt_res))
        return NULL;

    if (!NIL_P(py_stmt_res))
    {
        if (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
            return NULL;
        }
        else
        {
            stmt_res = (stmt_handle *)py_stmt_res;
        }
        return_str = ALLOC_N(char, DB_MAX_ERR_MSG_LEN);

        memset(return_str, 0, DB_MAX_ERR_MSG_LEN);

        _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, -1, 0,
                                        return_str, IDS_ERR,
                                        stmt_res->error_recno_tracker);

        if (stmt_res->error_recno_tracker - stmt_res->errormsg_recno_tracker >= 1)
        {
            stmt_res->errormsg_recno_tracker = stmt_res->error_recno_tracker;
        }
        stmt_res->error_recno_tracker++;

        retVal = StringOBJ_FromASCII(return_str);
        if (return_str != NULL)
        {
            PyMem_Del(return_str);
            return_str = NULL;
        }
        return retVal;
    }
    else
    {
        return StringOBJ_FromASCII(IFX_G(__python_stmt_err_state));
    }
}


/*!# IfxPy.num_fields
*
* ===Description
* int IfxPy.num_fields ( resource stmt )
*
* Returns the number of fields contained in a result set. This is most useful
* for handling the result sets returned by dynamically generated queries, or
* for result sets returned by stored procedures, where your application cannot
* otherwise know how to retrieve and use the results.
*
* ===Parameters
*
* ====stmt
*        A valid statement resource containing a result set.
*
* ===Return Values
*
* Returns an integer value representing the number of fields in the result set
* associated with the specified statement resource. Returns FALSE if the
* statement resource is not a valid input value.
*/
static PyObject *IfxPy_num_fields(PyObject *self, PyObject *args)
{
    PyObject *py_stmt_res = NULL;
    stmt_handle *stmt_res;
    int rc = 0;
    SQLSMALLINT indx = 0;
    char error [DB_MAX_ERR_MSG_LEN];

    if (!PyArg_ParseTuple(args, "O", &py_stmt_res))
        return NULL;

    if (!NIL_P(py_stmt_res))
    {
        if (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
            return NULL;
        }
        else
        {
            stmt_res = (stmt_handle *)py_stmt_res;
        }

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLNumResultCols((SQLHSTMT)stmt_res->hstmt, &indx);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc,
                                            1, NULL, -1, 1);
        }
        if (rc == SQL_ERROR)
        {
            sprintf(error, "SQLNumResultCols failed: %s",
                    IFX_G(__python_stmt_err_msg));
            PyErr_SetString(PyExc_Exception, error);
            return NULL;
        }
        return PyInt_FromLong(indx);
    }
    else
    {
        PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
        return NULL;
    }
    Py_INCREF(Py_False);
    return Py_False;
}

/*!# IfxPy.num_rows
*
* ===Description
* int IfxPy.num_rows ( resource stmt )
*
* Returns the number of rows deleted, inserted, or updated by an SQL statement.
*
* To determine the number of rows that will be returned by a SELECT statement,
* issue SELECT COUNT(*) with the same predicates as your intended SELECT
* statement and retrieve the value. If your application logic checks the number
* of rows returned by a SELECT statement and branches if the number of rows is
* 0, consider modifying your application to attempt to return the first row
* with one of IfxPy.fetch_assoc(), IfxPy.fetch_both(), IfxPy.fetch_array(),
* or IfxPy.fetch_row(), and branch if the fetch function returns FALSE.
*
* Note: If you issue a SELECT statement using a scrollable cursor,
* IfxPy.num_rows() returns the number of rows returned by the SELECT
* statement. However, the overhead associated with scrollable cursors
* significantly degrades the performance of your application, so if this is the
* only reason you are considering using scrollable cursors, you should use a
* forward-only cursor and either call SELECT COUNT(*) or rely on the boolean
* return value of the fetch functions to achieve the equivalent functionality
* with much better performance.
*
* ===Parameters
*
* ====stmt
*        A valid stmt resource containing a result set.
*
* ===Return Values
*
* Returns the number of rows affected by the last SQL statement issued by the
* specified statement handle.
*/
static PyObject *IfxPy_num_rows(PyObject *self, PyObject *args)
{
    PyObject *py_stmt_res = NULL;
    stmt_handle *stmt_res;
    int rc = 0;
    SQLLEN  count = 0;
    char error [DB_MAX_ERR_MSG_LEN];

    if (!PyArg_ParseTuple(args, "O", &py_stmt_res))
        return NULL;

    if (!NIL_P(py_stmt_res))
    {
        if (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
            return NULL;
        }
        else
        {
            stmt_res = (stmt_handle *)py_stmt_res;
        }

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLRowCount((SQLHSTMT)stmt_res->hstmt, &count);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc,
                                            1, NULL, -1, 1);
            sprintf(error, "SQLRowCount failed: %s",
                    IFX_G(__python_stmt_err_msg));
            PyErr_SetString(PyExc_Exception, error);
            return NULL;
        }
        return PyInt_FromLong(count);
    }
    else
    {
        PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
        return NULL;
    }
    Py_INCREF(Py_False);
    return Py_False;
}

/*!# IfxPy.get_num_result
*
* ===Description
* int IfxPy.num_rows ( resource stmt )
*
* Returns the number of rows in a current open non-dynamic scrollable cursor.
*
* ===Parameters
*
* ====stmt
*        A valid stmt resource containing a result set.
*
* ===Return Values
*
* True on success or False on failure.
*/
static PyObject *IfxPy_get_num_result(PyObject *self, PyObject *args)
{
    PyObject *py_stmt_res = NULL;
    stmt_handle *stmt_res;
    int rc = 0;
    SQLINTEGER count = 0;
    char error [DB_MAX_ERR_MSG_LEN];
    SQLSMALLINT strLenPtr;

    if (!PyArg_ParseTuple(args, "O", &py_stmt_res))
        return NULL;

    if (!NIL_P(py_stmt_res))
    {
        if (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
            return NULL;
        }
        else
        {
            stmt_res = (stmt_handle *)py_stmt_res;
        }

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetDiagField(SQL_HANDLE_STMT, stmt_res->hstmt, 0,
                             SQL_DIAG_CURSOR_ROW_COUNT, &count, SQL_IS_INTEGER,
                             &strLenPtr);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT,
                                            rc, 1, NULL, -1, 1);
        }
        if (rc == SQL_ERROR)
        {
            sprintf(error, "SQLGetDiagField failed: %s",
                    IFX_G(__python_stmt_err_msg));
            PyErr_SetString(PyExc_Exception, error);
            return NULL;
        }
        return PyInt_FromLong(count);
    }
    else
    {
        PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
        return NULL;
    }
    Py_INCREF(Py_False);
    return Py_False;
}

static int _python_IfxPy_get_column_by_name(stmt_handle *stmt_res, char *col_name, int col)
{
    int i;
    /* get column header info */
    if (stmt_res->column_info == NULL)
    {
        if (_python_IfxPy_get_result_set_info(stmt_res) < 0)
        {
            return -1;
        }
    }
    if (col_name == NULL)
    {
        if (col >= 0 && col < stmt_res->num_columns)
        {
            return col;
        }
        else
        {
            return -1;
        }
    }
    /* should start from 0 */
    i = 0;
    while (i < stmt_res->num_columns)
    {
        if (strcmp((char*)stmt_res->column_info [i].name, col_name) == 0)
        {
            return i;
        }
        i++;
    }
    return -1;
}

/*!# IfxPy.field_name
*
* ===Description
* string IfxPy.field_name ( resource stmt, mixed column )
*
* Returns the name of the specified column in the result set.
*
* ===Parameters
*
* ====stmt
*        Specifies a statement resource containing a result set.
*
* ====column
*        Specifies the column in the result set. This can either be an integer
* representing the 0-indexed position of the column, or a string containing the
* name of the column.
*
* ===Return Values
*
* Returns a string containing the name of the specified column. If the
* specified column does not exist in the result set, IfxPy.field_name()
* returns FALSE.
*/
static PyObject *IfxPy_field_name(PyObject *self, PyObject *args)
{
    PyObject *column = NULL;
#if  PY_MAJOR_VERSION >= 3
    PyObject *col_name_py3_tmp = NULL;
#endif
    PyObject *py_stmt_res = NULL;
    stmt_handle* stmt_res = NULL;
    char *col_name = NULL;
    int col = -1;

    if (!PyArg_ParseTuple(args, "OO", &py_stmt_res, &column))
        return NULL;

    if (NIL_P(py_stmt_res) || (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType)))
    {
        PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
        return NULL;
    }
    else
    {
        stmt_res = (stmt_handle *)py_stmt_res;
    }

    if (TYPE(column) == PYTHON_FIXNUM)
    {
        col = PyInt_AsLong(column);
    }
    else if (PyString_Check(column))
    {
#if  PY_MAJOR_VERSION >= 3
        col_name_py3_tmp = PyUnicode_AsASCIIString(column);
        if (col_name_py3_tmp == NULL)
        {
            return NULL;
        }
        column = col_name_py3_tmp;
#endif
        col_name = PyBytes_AsString(column);
    }
    else
    {
        /* Column argument has to be either an integer or string */
        Py_RETURN_FALSE;
    }
    col = _python_IfxPy_get_column_by_name(stmt_res, col_name, col);
#if  PY_MAJOR_VERSION >= 3
    if (col_name_py3_tmp != NULL)
    {
        Py_XDECREF(col_name_py3_tmp);
    }
#endif
    if (col < 0)
    {
        Py_INCREF(Py_False);
        return Py_False;
    }
    return StringOBJ_FromASCII((char*)stmt_res->column_info [col].name);
}

/*!# IfxPy.field_display_size
*
* ===Description
* int IfxPy.field_display_size ( resource stmt, mixed column )
*
* Returns the maximum number of bytes required to display a column in a result
* set.
*
* ===Parameters
* ====stmt
*        Specifies a statement resource containing a result set.
*
* ====column
*        Specifies the column in the result set. This can either be an integer
* representing the 0-indexed position of the column, or a string containing the
* name of the column.
*
* ===Return Values
*
* Returns an integer value with the maximum number of bytes required to display
* the specified column.
* If the column does not exist in the result set, IfxPy.field_display_size()
* returns FALSE.
*/
static PyObject *IfxPy_field_display_size(PyObject *self, PyObject *args)
{
    PyObject *py_stmt_res = NULL;
    PyObject *column = NULL;
#if  PY_MAJOR_VERSION >= 3
    PyObject *col_name_py3_tmp = NULL;
#endif
    int col = -1;
    char *col_name = NULL;
    stmt_handle *stmt_res = NULL;
    int rc;
    SQLLEN  colDataDisplaySize = 0;

    if (!PyArg_ParseTuple(args, "OO", &py_stmt_res, &column))
        return NULL;

    if (NIL_P(py_stmt_res) || (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType)))
    {
        PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
        return NULL;
    }
    else
    {
        stmt_res = (stmt_handle *)py_stmt_res;
    }

    if (TYPE(column) == PYTHON_FIXNUM)
    {
        col = PyInt_AsLong(column);
    }
    else if (PyString_Check(column))
    {
#if  PY_MAJOR_VERSION >= 3
        col_name_py3_tmp = PyUnicode_AsASCIIString(column);
        if (col_name_py3_tmp == NULL)
        {
            return NULL;
        }
        column = col_name_py3_tmp;
#endif
        col_name = PyBytes_AsString(column);
    }
    else
    {
        /* Column argument has to be either an integer or string */
        Py_RETURN_FALSE;
    }
    col = _python_IfxPy_get_column_by_name(stmt_res, col_name, col);
#if  PY_MAJOR_VERSION >= 3
    if (col_name_py3_tmp != NULL)
    {
        Py_XDECREF(col_name_py3_tmp);
    }
#endif
    if (col < 0)
    {
        Py_RETURN_FALSE;
    }

    Py_BEGIN_ALLOW_THREADS;

    // In ODBC 3.x, the ODBC 2.0 function SQLColAttributes has been replaced by SQLColAttribute
    // https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolattribute-function
    rc = SQLColAttribute((SQLHSTMT)stmt_res->hstmt, (SQLSMALLINT)col + 1,
                         SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL, &colDataDisplaySize);
    Py_END_ALLOW_THREADS;

    if (rc < SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO)
    {
        _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc, 1,
                                        NULL, -1, 1);
    }
    if (rc < SQL_SUCCESS)
    {
        Py_INCREF(Py_False);
        return Py_False;
    }
    return PyInt_FromLong(colDataDisplaySize);
}
/*!# IfxPy.field_nullable
*
* ===Description
* bool IfxPy.field_nullable ( resource stmt, mixed column )
*
* Returns True/False based on indicated column in result set is nullable or not.
*
* ===Parameters
*
* ====stmt
*              Specifies a statement resource containing a result set.
*
* ====column
*              Specifies the column in the result set. This can either be an integer
* representing the 0-indexed position of the column, or a string containing the
* name of the column.
*
* ===Return Values
*
* Returns TRUE if indicated column is nullable else returns FALSE.
* If the specified column does not exist in the result set, IfxPy.field_nullable() returns FALSE
*/
static PyObject *IfxPy_field_nullable(PyObject *self, PyObject *args)
{
    PyObject *py_stmt_res = NULL;
    PyObject *column = NULL;
#if  PY_MAJOR_VERSION >= 3
    PyObject *col_name_py3_tmp = NULL;
#endif
    stmt_handle* stmt_res = NULL;
    char *col_name = NULL;
    int col = -1;
    int rc;
    SQLLEN  nullableCol = 0;

    if (!PyArg_ParseTuple(args, "OO", &py_stmt_res, &column))
        return NULL;

    if (NIL_P(py_stmt_res) || (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType)))
    {
        PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
        return NULL;
    }
    else
    {
        stmt_res = (stmt_handle *)py_stmt_res;
    }

    if (TYPE(column) == PYTHON_FIXNUM)
    {
        col = PyInt_AsLong(column);
    }
    else if (PyString_Check(column))
    {
#if  PY_MAJOR_VERSION >= 3
        col_name_py3_tmp = PyUnicode_AsASCIIString(column);
        if (col_name_py3_tmp == NULL)
        {
            return NULL;
        }
        column = col_name_py3_tmp;
#endif
        col_name = PyBytes_AsString(column);
    }
    else
    {
        /* Column argument has to be either an integer or string */
        Py_RETURN_FALSE;
    }
    col = _python_IfxPy_get_column_by_name(stmt_res, col_name, col);
#if  PY_MAJOR_VERSION >= 3
    if (col_name_py3_tmp != NULL)
    {
        Py_XDECREF(col_name_py3_tmp);
    }
#endif
    if (col < 0)
    {
        Py_RETURN_FALSE;
    }

    Py_BEGIN_ALLOW_THREADS;
    rc = SQLColAttribute((SQLHSTMT)stmt_res->hstmt, (SQLSMALLINT)col + 1,
                         SQL_DESC_NULLABLE, NULL, 0, NULL, &nullableCol);
    Py_END_ALLOW_THREADS;

    if (rc < SQL_SUCCESS)
    {
        _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc, 1,
                                        NULL, -1, 1);
        Py_RETURN_FALSE;
    }
    else if (rc == SQL_SUCCESS_WITH_INFO)
    {
        _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc, 1,
                                        NULL, -1, 1);
    }
    else if (nullableCol == SQL_NULLABLE)
    {
        Py_RETURN_TRUE;
    }
    else
    {
        Py_RETURN_FALSE;
    }

    Py_RETURN_FALSE;
}


/*!# IfxPy.field_num
*
* ===Description
* int IfxPy.field_num ( resource stmt, mixed column )
*
* Returns the position of the named column in a result set.
*
* ===Parameters
*
* ====stmt
*        Specifies a statement resource containing a result set.
*
* ====column
*        Specifies the column in the result set. This can either be an integer
* representing the 0-indexed position of the column, or a string containing the
* name of the column.
*
* ===Return Values
*
* Returns an integer containing the 0-indexed position of the named column in
* the result set. If the specified column does not exist in the result set,
* IfxPy.field_num() returns FALSE.
*/

static PyObject *IfxPy_field_num(PyObject *self, PyObject *args)
{
    PyObject *py_stmt_res = NULL;
    PyObject *column = NULL;
#if  PY_MAJOR_VERSION >= 3
    PyObject *col_name_py3_tmp = NULL;
#endif
    stmt_handle* stmt_res = NULL;
    char *col_name = NULL;
    int col = -1;

    if (!PyArg_ParseTuple(args, "OO", &py_stmt_res, &column))
        return NULL;

    if (NIL_P(py_stmt_res) || (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType)))
    {
        PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
        return NULL;
    }
    else
    {
        stmt_res = (stmt_handle *)py_stmt_res;
    }

    if (TYPE(column) == PYTHON_FIXNUM)
    {
        col = PyInt_AsLong(column);
    }
    else if (PyString_Check(column))
    {
#if  PY_MAJOR_VERSION >= 3
        col_name_py3_tmp = PyUnicode_AsASCIIString(column);
        if (col_name_py3_tmp == NULL)
        {
            return NULL;
        }
        column = col_name_py3_tmp;
#endif
        col_name = PyBytes_AsString(column);
    }
    else
    {
        /* Column argument has to be either an integer or string */
        Py_RETURN_FALSE;
    }
    col = _python_IfxPy_get_column_by_name(stmt_res, col_name, col);
#if  PY_MAJOR_VERSION >= 3
    if (col_name_py3_tmp != NULL)
    {
        Py_XDECREF(col_name_py3_tmp);
    }
#endif
    if (col < 0)
    {
        Py_INCREF(Py_False);
        return Py_False;
    }
    return PyInt_FromLong(col);
}

/*!# IfxPy.field_precision
*
* ===Description
* int IfxPy.field_precision ( resource stmt, mixed column )
*
* Returns the precision of the indicated column in a result set.
*
* ===Parameters
*
* ====stmt
*        Specifies a statement resource containing a result set.
*
* ====column
*        Specifies the column in the result set. This can either be an integer
* representing the 0-indexed position of the column, or a string containing the
* name of the column.
*
* ===Return Values
*
* Returns an integer containing the precision of the specified column. If the
* specified column does not exist in the result set, IfxPy.field_precision()
* returns FALSE.
*/
static PyObject *IfxPy_field_precision(PyObject *self, PyObject *args)
{
    PyObject *py_stmt_res = NULL;
    PyObject *column = NULL;
#if  PY_MAJOR_VERSION >= 3
    PyObject *col_name_py3_tmp = NULL;
#endif
    stmt_handle* stmt_res = NULL;
    char *col_name = NULL;
    int col = -1;

    if (!PyArg_ParseTuple(args, "OO", &py_stmt_res, &column))
        return NULL;

    if (NIL_P(py_stmt_res) || (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType)))
    {
        PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
        return NULL;
    }
    else
    {
        stmt_res = (stmt_handle *)py_stmt_res;
    }

    if (TYPE(column) == PYTHON_FIXNUM)
    {
        col = PyInt_AsLong(column);
    }
    else if (PyString_Check(column))
    {
#if  PY_MAJOR_VERSION >= 3
        col_name_py3_tmp = PyUnicode_AsASCIIString(column);
        if (col_name_py3_tmp == NULL)
        {
            return NULL;
        }
        column = col_name_py3_tmp;
#endif
        col_name = PyBytes_AsString(column);
    }
    else
    {
        /* Column argument has to be either an integer or string */
        Py_RETURN_FALSE;
    }
    col = _python_IfxPy_get_column_by_name(stmt_res, col_name, col);
#if  PY_MAJOR_VERSION >= 3
    if (col_name_py3_tmp != NULL)
    {
        Py_XDECREF(col_name_py3_tmp);
    }
#endif
    if (col < 0)
    {
        Py_RETURN_FALSE;
    }
    return PyInt_FromLong(stmt_res->column_info [col].size);

}

/*!# IfxPy.field_scale
*
* ===Description
* int IfxPy.field_scale ( resource stmt, mixed column )
*
* Returns the scale of the indicated column in a result set.
*
* ===Parameters
* ====stmt
*        Specifies a statement resource containing a result set.
*
* ====column
*        Specifies the column in the result set. This can either be an integer
* representing the 0-indexed position of the column, or a string containing the
* name of the column.
*
* ===Return Values
*
* Returns an integer containing the scale of the specified column. If the
* specified column does not exist in the result set, IfxPy.field_scale()
* returns FALSE.
*/
static PyObject *IfxPy_field_scale(PyObject *self, PyObject *args)
{
    PyObject *py_stmt_res = NULL;
    PyObject *column = NULL;
#if  PY_MAJOR_VERSION >= 3
    PyObject *col_name_py3_tmp = NULL;
#endif
    stmt_handle* stmt_res = NULL;
    char *col_name = NULL;
    int col = -1;

    if (!PyArg_ParseTuple(args, "OO", &py_stmt_res, &column))
        return NULL;

    if (NIL_P(py_stmt_res) || (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType)))
    {
        PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
        return NULL;
    }
    else
    {
        stmt_res = (stmt_handle *)py_stmt_res;
    }
    if (TYPE(column) == PYTHON_FIXNUM)
    {
        col = PyInt_AsLong(column);
    }
    else if (PyString_Check(column))
    {
#if  PY_MAJOR_VERSION >= 3
        col_name_py3_tmp = PyUnicode_AsASCIIString(column);
        if (col_name_py3_tmp == NULL)
        {
            return NULL;
        }
        column = col_name_py3_tmp;
#endif
        col_name = PyBytes_AsString(column);
    }
    else
    {
        /* Column argument has to be either an integer or string */
        Py_RETURN_FALSE;
    }
    col = _python_IfxPy_get_column_by_name(stmt_res, col_name, col);
#if  PY_MAJOR_VERSION >= 3
    if (col_name_py3_tmp != NULL)
    {
        Py_XDECREF(col_name_py3_tmp);
    }
#endif
    if (col < 0)
    {
        Py_RETURN_FALSE;
    }
    return PyInt_FromLong(stmt_res->column_info [col].scale);
}

/*!# IfxPy.field_type
*
* ===Description
* string IfxPy.field_type ( resource stmt, mixed column )
*
* Returns the data type of the indicated column in a result set.
*
* ===Parameters
* ====stmt
*        Specifies a statement resource containing a result set.
*
* ====column
*        Specifies the column in the result set. This can either be an integer
* representing the 0-indexed position of the column, or a string containing the
* name of the column.
*
* ====Return Values
*
* Returns a string containing the defined data type of the specified column.
* If the specified column does not exist in the result set, IfxPy.field_type()
* returns FALSE.
*/
static PyObject *IfxPy_field_type(PyObject *self, PyObject *args)
{
    PyObject *py_stmt_res = NULL;
    PyObject *column = NULL;
#if  PY_MAJOR_VERSION >= 3
    PyObject *col_name_py3_tmp = NULL;
#endif
    stmt_handle* stmt_res = NULL;
    char *col_name = NULL;
    char *str_val = NULL;
    int col = -1;

    if (!PyArg_ParseTuple(args, "OO", &py_stmt_res, &column))
        return NULL;

    if (NIL_P(py_stmt_res) || (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType)))
    {
        PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
        return NULL;
    }
    else
    {
        stmt_res = (stmt_handle *)py_stmt_res;
    }
    if (TYPE(column) == PYTHON_FIXNUM)
    {
        col = PyInt_AsLong(column);
    }
    else if (PyString_Check(column))
    {
#if  PY_MAJOR_VERSION >= 3
        col_name_py3_tmp = PyUnicode_AsASCIIString(column);
        if (col_name_py3_tmp == NULL)
        {
            return NULL;
        }
        column = col_name_py3_tmp;
#endif
        col_name = PyBytes_AsString(column);
    }
    else
    {
        /* Column argument has to be either an integer or string */
        Py_RETURN_FALSE;
    }
    col = _python_IfxPy_get_column_by_name(stmt_res, col_name, col);
#if  PY_MAJOR_VERSION >= 3
    if (col_name_py3_tmp != NULL)
    {
        Py_XDECREF(col_name_py3_tmp);
    }
#endif
    if (col < 0)
    {
        Py_RETURN_FALSE;
    }
    switch (stmt_res->column_info [col].type)
    {
    case SQL_SMALLINT:
    case SQL_INTEGER:
        str_val = "int";
        break;
    case SQL_BIGINT:
        str_val = "bigint";
        break;
    case SQL_REAL:
    case SQL_FLOAT:
    case SQL_DOUBLE:
        str_val = "real";
        break;
    case SQL_DECIMAL:
    case SQL_NUMERIC:
        str_val = "decimal";
        break;

    case SQL_TYPE_DATE:
        str_val = "date";
        break;
    case SQL_TYPE_TIME:
        str_val = "time";
        break;
    case SQL_TYPE_TIMESTAMP:
        str_val = "timestamp";
        break;
    case SQL_INTERVAL_DAY:
    case SQL_INTERVAL_HOUR:
    case SQL_INTERVAL_MINUTE:
    case SQL_INTERVAL_SECOND:
    case SQL_INTERVAL_DAY_TO_HOUR:
    case SQL_INTERVAL_DAY_TO_MINUTE:
    case SQL_INTERVAL_DAY_TO_SECOND:
    case SQL_INTERVAL_HOUR_TO_MINUTE:
    case SQL_INTERVAL_HOUR_TO_SECOND:
    case SQL_INTERVAL_MINUTE_TO_SECOND:
        str_val = "interval";
        break;
    case SQL_INFX_RC_SET:
	    str_val = "set";
	    break;
    case SQL_INFX_RC_MULTISET:
	    str_val = "multiset";
	    break;
    case SQL_INFX_RC_ROW:
	    str_val = "row";
	    break;
    case SQL_INFX_RC_LIST:
	    str_val = "list";
	    break;
    case SQL_INFX_RC_COLLECTION:
	    str_val = "collection";
	    break;
    case SQL_INFX_UDT_FIXED:
	    str_val = "udt_fixed";
	    break;
    case SQL_INFX_UDT_VARYING:
	    str_val = "udt_varying";
	    break;
    default:
        str_val = "string";
        break;
    }
    return StringOBJ_FromASCII(str_val);
}

/*!# IfxPy.field_width
*
* ===Description
* int IfxPy.field_width ( resource stmt, mixed column )
*
* Returns the width of the current value of the indicated column in a result
* set. This is the maximum width of the column for a fixed-length data type, or
* the actual width of the column for a variable-length data type.
*
* ===Parameters
*
* ====stmt
*        Specifies a statement resource containing a result set.
*
* ====column
*        Specifies the column in the result set. This can either be an integer
* representing the 0-indexed position of the column, or a string containing the
* name of the column.
*
* ===Return Values
*
* Returns an integer containing the width of the specified character or binary
* data type column in a result set. If the specified column does not exist in
* the result set, IfxPy.field_width() returns FALSE.
*/
static PyObject *IfxPy_field_width(PyObject *self, PyObject *args)
{
    PyObject *py_stmt_res = NULL;
    PyObject *column = NULL;
#if  PY_MAJOR_VERSION >= 3
    PyObject *col_name_py3_tmp = NULL;
#endif
    int col = -1;
    char *col_name = NULL;
    stmt_handle *stmt_res = NULL;
    int rc;
    SQLLEN  colDataSize = 0;

    if (!PyArg_ParseTuple(args, "OO", &py_stmt_res, &column))
        return NULL;

    if (NIL_P(py_stmt_res) || (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType)))
    {
        PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
        return NULL;
    }
    else
    {
        stmt_res = (stmt_handle *)py_stmt_res;
    }
    if (TYPE(column) == PYTHON_FIXNUM)
    {
        col = PyInt_AsLong(column);
    }
    else if (PyString_Check(column))
    {
#if  PY_MAJOR_VERSION >= 3
        col_name_py3_tmp = PyUnicode_AsASCIIString(column);
        if (col_name_py3_tmp == NULL)
        {
            return NULL;
        }
        column = col_name_py3_tmp;
#endif
        col_name = PyBytes_AsString(column);
    }
    else
    {
        /* Column argument has to be either an integer or string */
        Py_RETURN_FALSE;
    }
    col = _python_IfxPy_get_column_by_name(stmt_res, col_name, col);
#if  PY_MAJOR_VERSION >= 3
    if (col_name_py3_tmp != NULL)
    {
        Py_XDECREF(col_name_py3_tmp);
    }
#endif
    if (col < 0)
    {
        Py_RETURN_FALSE;
    }

    Py_BEGIN_ALLOW_THREADS;
    rc = SQLColAttribute((SQLHSTMT)stmt_res->hstmt, (SQLSMALLINT)col + 1,
                         SQL_DESC_LENGTH, NULL, 0, NULL, &colDataSize);
    Py_END_ALLOW_THREADS;

    if (rc != SQL_SUCCESS)
    {
        _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc, 1,
                                        NULL, -1, 1);
        PyErr_Clear();
        Py_RETURN_FALSE;
    }
    return PyInt_FromLong(colDataSize);
}

/*!# IfxPy.cursor_type
*
* ===Description
* int IfxPy.cursor_type ( resource stmt )
*
* Returns the cursor type used by a statement resource. Use this to determine
* if you are working with a forward-only cursor or scrollable cursor.
*
* ===Parameters
* ====stmt
*        A valid statement resource.
*
* ===Return Values
*
* Returns either SQL_SCROLL_FORWARD_ONLY if the statement resource uses a
* forward-only cursor or SQL_CURSOR_KEYSET_DRIVEN if the statement resource
* uses a scrollable cursor.
*/
static PyObject *IfxPy_cursor_type(PyObject *self, PyObject *args)
{
    PyObject *py_stmt_res = NULL;
    stmt_handle *stmt_res = NULL;

    if (!PyArg_ParseTuple(args, "O", &py_stmt_res))
        return NULL;

    if (NIL_P(py_stmt_res) || (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType)))
    {
        PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
        return NULL;
    }
    else
    {
        stmt_res = (stmt_handle *)py_stmt_res;
    }

    return PyInt_FromLong(stmt_res->cursor_type != SQL_SCROLL_FORWARD_ONLY);
}

/*!# IfxPy.rollback
*
* ===Description
* bool IfxPy.rollback ( resource connection )
*
* Rolls back an in-progress transaction on the specified connection resource
* and begins a new transaction. Python applications normally default to
* AUTOCOMMIT mode, so IfxPy.rollback() normally has no effect unless
* AUTOCOMMIT has been turned off for the connection resource.
*
* Note: If the specified connection resource is a persistent connection, all
* transactions in progress for all applications using that persistent
* connection will be rolled back. For this reason, persistent connections are
* not recommended for use in applications that require transactions.
*
* ===Parameters
*
* ====connection
*        A valid database connection resource variable as returned from
* IfxPy.connect() 
*
* ===Return Values
*
* Returns TRUE on success or FALSE on failure.
*/
static PyObject *IfxPy_rollback(PyObject *self, PyObject *args)
{
    PyObject *py_conn_res = NULL;
    conn_handle *conn_res;
    int rc;

    if (!PyArg_ParseTuple(args, "O", &py_conn_res))
        return NULL;

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }
        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            return NULL;
        }

        rc = SQLEndTran(SQL_HANDLE_DBC, conn_res->hdbc, SQL_ROLLBACK);

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            Py_RETURN_TRUE;
        }
    }
    Py_RETURN_FALSE;
}

/*!# IfxPy.free_stmt
*
* ===Description
* bool IfxPy.free_stmt ( resource stmt )
*
* Frees the system and database resources that are associated with a statement
* resource. These resources are freed implicitly when a script finishes, but
* you can call IfxPy.free_stmt() to explicitly free the statement resources
* before the end of the script.
*
* ===Parameters
* ====stmt
*        A valid statement resource.
*
* ===Return Values
*
* Returns TRUE on success or FALSE on failure.
*
* DEPRECATED
*/
static PyObject *IfxPy_free_stmt(PyObject *self, PyObject *args)
{
    PyObject    *py_stmt_res = NULL;
    stmt_handle *handle = NULL;
    SQLRETURN   rc = 0;

    if (!PyArg_ParseTuple(args, "O", &py_stmt_res))
        return NULL;
    if (!NIL_P(py_stmt_res))
    {
        if (PyObject_TypeCheck(py_stmt_res, &stmt_handleType))
        {
            handle = (stmt_handle *)py_stmt_res;
            if (handle->hstmt != NULL)
            {
                rc = SQLFreeHandle(SQL_HANDLE_STMT, handle->hstmt);

                if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
                {
                    _python_IfxPy_check_sql_errors(handle->hstmt,
                                                    SQL_HANDLE_STMT,
                                                    rc, 1, NULL, -1, 1);
                }
                if (rc == SQL_ERROR)
                {
                    Py_RETURN_FALSE;
                }
                _python_IfxPy_free_result_struct(handle);
                handle->hstmt = NULL;
                Py_RETURN_TRUE;
            }
        }
    }
    Py_RETURN_NONE;
}

static RETCODE _python_IfxPy_get_data(stmt_handle *stmt_res, int col_num, short ctype, void *buff, SQLLEN in_length, SQLLEN *out_length)
{
    RETCODE rc = SQL_SUCCESS;

    Py_BEGIN_ALLOW_THREADS;
    rc = SQLGetData((SQLHSTMT)stmt_res->hstmt, col_num, ctype, buff, in_length,
                    out_length);
    Py_END_ALLOW_THREADS;

    if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
    {
        _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc, 1,
                                        NULL, -1, 1);
    }
    return rc;
}

/*!# IfxPy.result
*
* ===Description
* mixed IfxPy.result ( resource stmt, mixed column )
*
* Returns a single column from a row in the result set
*
* Use IfxPy.result() to return the value of a specified column in the current  * row of a result set. You must call IfxPy.fetch_row() before calling
* IfxPy.result() to set the location of the result set pointer.
*
* ===Parameters
*
* ====stmt
*        A valid stmt resource.
*
* ====column
*        Either an integer mapping to the 0-indexed field in the result set, or  * a string matching the name of the column.
*
* ===Return Values
*
* Returns the value of the requested field if the field exists in the result
* set. Returns NULL if the field does not exist, and issues a warning.
*/
static PyObject *IfxPy_result(PyObject *self, PyObject *args)
{
    PyObject *py_stmt_res = NULL;
    PyObject *column = NULL;
#if  PY_MAJOR_VERSION >= 3
    PyObject *col_name_py3_tmp = NULL;
#endif
    PyObject *retVal = NULL;
    stmt_handle *stmt_res;
    long col_num;
    RETCODE rc;
    void    *out_ptr;
    DATE_STRUCT *date_ptr;
    TIME_STRUCT *time_ptr;
    TIMESTAMP_STRUCT *ts_ptr;
    SQL_INTERVAL_STRUCT *interval_ptr;
    char error [DB_MAX_ERR_MSG_LEN];
    SQLULEN in_length;
    SQLLEN out_length = 0;

    SQLSMALLINT column_type, targetCType = SQL_C_CHAR, len_terChar = 0;
    double double_val;
    SQLINTEGER long_val;
    PyObject *return_value = NULL;

    if (!PyArg_ParseTuple(args, "OO", &py_stmt_res, &column))
        return NULL;

    if (!NIL_P(py_stmt_res))
    {
        if (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
            return NULL;
        }
        else
        {
            stmt_res = (stmt_handle *)py_stmt_res;
        }

        if (TYPE(column) == PYTHON_FIXNUM)
        {
            col_num = PyLong_AsLong(column);

        }
        else if (PyString_Check(column))
        {
#if  PY_MAJOR_VERSION >= 3
            col_name_py3_tmp = PyUnicode_AsASCIIString(column);
            if (col_name_py3_tmp == NULL)
            {
                return NULL;
            }
            column = col_name_py3_tmp;
#endif
            col_num = _python_IfxPy_get_column_by_name(stmt_res, PyBytes_AsString(column), -1);
#if  PY_MAJOR_VERSION >= 3
            if (col_name_py3_tmp != NULL)
            {
                Py_XDECREF(col_name_py3_tmp);
            }
#endif
        }
        else
        {
            /* Column argument has to be either an integer or string */
            Py_RETURN_FALSE;
        }

        /* get column header info */
        if (stmt_res->column_info == NULL)
        {
            if (_python_IfxPy_get_result_set_info(stmt_res) < 0)
            {
                sprintf(error, "Column information cannot be retrieved: %s",
                        IFX_G(__python_stmt_err_msg));
                strcpy(IFX_G(__python_stmt_err_msg), error);
                PyErr_Clear();
                Py_RETURN_FALSE;
            }
        }

        if (col_num < 0 || col_num >= stmt_res->num_columns)
        {
            strcpy(IFX_G(__python_stmt_err_msg), "Column ordinal out of range");
            PyErr_Clear();
            Py_RETURN_NONE;
        }

        /* get the data */
        column_type = stmt_res->column_info [col_num].type;
        switch (column_type)
        {
        case SQL_CHAR:
        case SQL_VARCHAR:
        case SQL_WCHAR:
        case SQL_WVARCHAR:
        case SQL_LONGVARCHAR:
        case SQL_BIGINT:
        case SQL_DECIMAL:
        case SQL_NUMERIC:
        case SQL_INFX_RC_SET:
        case SQL_INFX_RC_MULTISET:	
	case SQL_INFX_RC_COLLECTION:
	case SQL_INFX_RC_ROW:
	case SQL_INFX_RC_LIST:
	case SQL_INFX_UDT_FIXED:
	case SQL_INFX_UDT_VARYING:
            if (column_type == SQL_DECIMAL || column_type == SQL_NUMERIC)
            {
                in_length = stmt_res->column_info [col_num].size +
                    stmt_res->column_info [col_num].scale + 2 + 1;
            }
            else
            {
                in_length = stmt_res->column_info [col_num].size + 1;
            }
            out_ptr = (SQLPOINTER)ALLOC_N(Py_UNICODE, in_length);

            if (out_ptr == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                return NULL;
            }

            rc = _python_IfxPy_get_data(stmt_res, col_num + 1, SQL_C_WCHAR,
                                         out_ptr, in_length * sizeof(Py_UNICODE), &out_length);

            if (rc == SQL_ERROR)
            {
                if (out_ptr != NULL)
                {
                    PyMem_Del(out_ptr);
                    out_ptr = NULL;
                }
                PyErr_Clear();
                Py_RETURN_FALSE;
            }
            if (out_length == SQL_NULL_DATA)
            {
                Py_INCREF(Py_None);
                return_value = Py_None;
            } //else if (column_type == SQL_BIGINT){
              //    return_value = PyLong_FromString(out_ptr, NULL, 0); }
              // Converting from Wchar string to long leads to data truncation
              // as it treats 00 in 2 bytes for each char as NULL
            else
            {
                return_value = getSQLWCharAsPyUnicodeObject(out_ptr, out_length);
            }
            PyMem_Del(out_ptr);
            out_ptr = NULL;
            return return_value;

        case SQL_TYPE_DATE:
            date_ptr = ALLOC(DATE_STRUCT);
            if (date_ptr == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                return NULL;
            }

            rc = _python_IfxPy_get_data(stmt_res, col_num + 1, SQL_C_TYPE_DATE,
                                         date_ptr, sizeof(DATE_STRUCT), &out_length);

            if (rc == SQL_ERROR)
            {
                if (date_ptr != NULL)
                {
                    PyMem_Del(date_ptr);
                    date_ptr = NULL;
                }
                PyErr_Clear();
                Py_RETURN_FALSE;
            }
            if (out_length == SQL_NULL_DATA)
            {
                PyMem_Del(date_ptr);
                date_ptr = NULL;
                Py_RETURN_NONE;
            }
            else
            {
                return_value = PyDate_FromDate(date_ptr->year, date_ptr->month, date_ptr->day);
                PyMem_Del(date_ptr);
                date_ptr = NULL;
                return return_value;
            }
            break;

        case SQL_TYPE_TIME:
            time_ptr = ALLOC(TIME_STRUCT);
            if (time_ptr == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                return NULL;
            }

            rc = _python_IfxPy_get_data(stmt_res, col_num + 1, SQL_C_TYPE_TIME,
                                         time_ptr, sizeof(TIME_STRUCT), &out_length);

            if (rc == SQL_ERROR)
            {
                if (time_ptr != NULL)
                {
                    PyMem_Del(time_ptr);
                    time_ptr = NULL;
                }
                PyErr_Clear();
                Py_RETURN_FALSE;
            }

            if (out_length == SQL_NULL_DATA)
            {
                PyMem_Del(time_ptr);
                time_ptr = NULL;
                Py_RETURN_NONE;
            }
            else
            {
                return_value = PyTime_FromTime(time_ptr->hour, time_ptr->minute, time_ptr->second, 0);
                PyMem_Del(time_ptr);
                time_ptr = NULL;
                return return_value;
            }
            break;

        case SQL_TYPE_TIMESTAMP:
            ts_ptr = ALLOC(TIMESTAMP_STRUCT);
            if (ts_ptr == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                return NULL;
            }

            rc = _python_IfxPy_get_data(stmt_res, col_num + 1, SQL_C_TYPE_TIMESTAMP,
                                         ts_ptr, sizeof(TIMESTAMP_STRUCT), &out_length);

            if (rc == SQL_ERROR)
            {
                if (ts_ptr != NULL)
                {
                    PyMem_Del(ts_ptr);
                    time_ptr = NULL;
                }
                PyErr_Clear();
                Py_RETURN_FALSE;
            }

            if (out_length == SQL_NULL_DATA)
            {
                PyMem_Del(ts_ptr);
                ts_ptr = NULL;
                Py_RETURN_NONE;
            }
            else
            {
                return_value = PyDateTime_FromDateAndTime(ts_ptr->year, ts_ptr->month, ts_ptr->day, ts_ptr->hour, ts_ptr->minute, ts_ptr->second, ts_ptr->fraction / 1000);
                PyMem_Del(ts_ptr);
                ts_ptr = NULL;
                return return_value;
            }
            break;

        case SQL_INTERVAL_DAY:
        case SQL_INTERVAL_HOUR:
        case SQL_INTERVAL_MINUTE:
        case SQL_INTERVAL_SECOND:
        case SQL_INTERVAL_DAY_TO_HOUR:
        case SQL_INTERVAL_DAY_TO_MINUTE:
        case SQL_INTERVAL_DAY_TO_SECOND:
        case SQL_INTERVAL_HOUR_TO_MINUTE:
        case SQL_INTERVAL_HOUR_TO_SECOND:
        case SQL_INTERVAL_MINUTE_TO_SECOND:
            interval_ptr = ALLOC(SQL_INTERVAL_STRUCT);
            if (interval_ptr == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                return NULL;
            }

            rc = _python_IfxPy_get_data(stmt_res, col_num + 1, column_type,
                                         interval_ptr, sizeof(SQL_INTERVAL_STRUCT), &out_length);

            if (rc == SQL_ERROR)
            {
                if (interval_ptr != NULL)
                {
                    PyMem_Del(interval_ptr);
                    time_ptr = NULL;
                }
                PyErr_Clear();
                Py_RETURN_FALSE;
            }

            if (out_length == SQL_NULL_DATA)
            {
                PyMem_Del(interval_ptr);
                interval_ptr = NULL;
                Py_RETURN_NONE;
            }
            else
            {
                if (interval_ptr->interval_sign == 0) {
                  return_value = PyDelta_FromDSU((interval_ptr->intval.day_second.day * -1) -1,
                                                 (86400 -
                                                  (interval_ptr->intval.day_second.hour * 3600) +
                                                  (interval_ptr->intval.day_second.minute * 60) +
                                                  interval_ptr->intval.day_second.second), 0);
                } else {
                  return_value = PyDelta_FromDSU(interval_ptr->intval.day_second.day,
                                                 ((interval_ptr->intval.day_second.hour * 3600) +
                                                  (interval_ptr->intval.day_second.minute * 60) +
                                                  interval_ptr->intval.day_second.second), 0);
                }
                PyMem_Del(interval_ptr);
                interval_ptr = NULL;
                return return_value;
            }
            break;

        case SQL_BIT:
        case SQL_SMALLINT:
        case SQL_INTEGER:
            rc = _python_IfxPy_get_data(stmt_res, col_num + 1, SQL_C_LONG,
                                         &long_val, sizeof(long_val),
                                         &out_length);
            if (rc == SQL_ERROR)
            {
                PyErr_Clear();
                Py_RETURN_FALSE;
            }
            if (out_length == SQL_NULL_DATA)
            {
                Py_RETURN_NONE;
            }
            else
            {
                return PyInt_FromLong(long_val);
            }
            break;

        case SQL_REAL:
        case SQL_FLOAT:
        case SQL_DOUBLE:
            rc = _python_IfxPy_get_data(stmt_res, col_num + 1, SQL_C_DOUBLE,
                                         &double_val, sizeof(double_val),
                                         &out_length);
            if (rc == SQL_ERROR)
            {
                PyErr_Clear();
                Py_RETURN_FALSE;
            }
            if (out_length == SQL_NULL_DATA)
            {
                Py_RETURN_NONE;
            }
            else
            {
                return PyFloat_FromDouble(double_val);
            }
            break;

        case SQL_BINARY:
        case SQL_LONGVARBINARY:
        case SQL_VARBINARY:
            switch (stmt_res->s_bin_mode)
            {
            case PASSTHRU:
                return PyBytes_FromStringAndSize("", 0);
                break;
                /* returns here */
            case CONVERT:
                targetCType = SQL_C_CHAR;
                len_terChar = sizeof(char);
                break;
            case BINARY:
                targetCType = SQL_C_BINARY;
                len_terChar = 0;
                break;
            default:
                Py_RETURN_FALSE;
            }

            out_ptr = ALLOC_N(char, INIT_BUFSIZ + len_terChar);
            if (out_ptr == NULL)
            {
                PyErr_SetString(PyExc_Exception,
                                "Failed to Allocate Memory for XML Data");
                return NULL;
            }
            rc = _python_IfxPy_get_data(stmt_res, col_num + 1, targetCType, out_ptr,
                                         INIT_BUFSIZ + len_terChar, &out_length);
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                void *tmp_out_ptr = NULL;

                tmp_out_ptr = ALLOC_N(char, out_length + INIT_BUFSIZ + len_terChar);
                memcpy(tmp_out_ptr, out_ptr, INIT_BUFSIZ);
                PyMem_Del(out_ptr);
                out_ptr = tmp_out_ptr;

                rc = _python_IfxPy_get_data(stmt_res, col_num + 1, targetCType, (char *)out_ptr + INIT_BUFSIZ,
                                             out_length + len_terChar, &out_length);
                if (rc == SQL_ERROR)
                {
                    PyMem_Del(out_ptr);
                    out_ptr = NULL;
                    return NULL;
                }
                if (len_terChar == sizeof(SQLWCHAR))
                {
                    retVal = getSQLWCharAsPyUnicodeObject(out_ptr, INIT_BUFSIZ + out_length);
                }
                else
                {
                    retVal = PyBytes_FromStringAndSize((char *)out_ptr, INIT_BUFSIZ + out_length);
                }
            }
            else if (rc == SQL_ERROR)
            {
                PyMem_Del(out_ptr);
                out_ptr = NULL;
                Py_RETURN_FALSE;
            }
            else
            {
                if (out_length == SQL_NULL_DATA)
                {
                    Py_INCREF(Py_None);
                    retVal = Py_None;
                }
                else
                {
                    if (len_terChar == 0)
                    {
                        retVal = PyBytes_FromStringAndSize((char *)out_ptr, out_length);
                    }
                    else
                    {
                        retVal = getSQLWCharAsPyUnicodeObject(out_ptr, out_length);
                    }
                }

            }
            if (out_ptr != NULL)
            {
                PyMem_Del(out_ptr);
                out_ptr = NULL;
            }
            return retVal;
        default:
            break;
        }
    }
    else
    {
        PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
    }
    Py_RETURN_FALSE;
}

static PyObject *_python_IfxPy_bind_fetch_helper(PyObject *args, int op)
{
    int rc = -1;
    int column_number;
    SQLINTEGER row_number = -1;
    stmt_handle *stmt_res = NULL;
    SQLSMALLINT column_type;
    IfxPy_row_data_type *row_data;
    SQLULEN tmp_length = 0;
    SQLLEN out_length = 0;
    void *out_ptr = NULL;
    SQLWCHAR *wout_ptr = NULL;
    int len_terChar = 0;
    SQLSMALLINT targetCType = SQL_C_CHAR;
    PyObject *py_stmt_res = NULL;
    PyObject *return_value = NULL;
    PyObject *key = NULL;
    PyObject *value = NULL;
    PyObject *py_row_number = NULL;
    char error [DB_MAX_ERR_MSG_LEN];

    if (!PyArg_ParseTuple(args, "O|O", &py_stmt_res, &py_row_number))
        return NULL;

    if (NIL_P(py_stmt_res) || (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType)))
    {
        PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
        return NULL;
    }
    else
    {
        stmt_res = (stmt_handle *)py_stmt_res;
    }

    if (!NIL_P(py_row_number))
    {
        if (PyInt_Check(py_row_number))
        {
            row_number = (SQLINTEGER)PyInt_AsLong(py_row_number);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
            return NULL;
        }
    }
    _python_IfxPy_init_error_info(stmt_res);

    /* get column header info */
    if (stmt_res->column_info == NULL)
    {
        if (_python_IfxPy_get_result_set_info(stmt_res) < 0)
        {
            sprintf(error, "Column information cannot be retrieved: %s",
                    IFX_G(__python_stmt_err_msg));
            PyErr_SetString(PyExc_Exception, error);
            return NULL;
        }
    }
    /* bind the data */
    if (stmt_res->row_data == NULL)
    {
        rc = _python_IfxPy_bind_column_helper(stmt_res);
        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
        {
            sprintf(error, "Column binding cannot be done: %s",
                    IFX_G(__python_stmt_err_msg));
            PyErr_SetString(PyExc_Exception, error);
            return NULL;
        }
    }
    /* check if row_number is present */
    if (PyTuple_Size(args) == 2 && row_number > 0)
    {
        Py_BEGIN_ALLOW_THREADS;
        rc = SQLFetchScroll((SQLHSTMT)stmt_res->hstmt, SQL_FETCH_ABSOLUTE,
                            row_number);
        if (rc == SQL_SUCCESS_WITH_INFO)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT,
                                            rc, 1,
                                            NULL, -1, 1);
        }
        Py_END_ALLOW_THREADS;
    }
    else if (PyTuple_Size(args) == 2 && row_number < 0)
    {
        PyErr_SetString(PyExc_Exception,
                        "Requested row number must be a positive value");
        return NULL;
    }
    else
    {
        /* row_number is NULL or 0; just fetch next row */
        Py_BEGIN_ALLOW_THREADS;

        rc = SQLFetch((SQLHSTMT)stmt_res->hstmt);
        if (rc == SQL_SUCCESS_WITH_INFO)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT,
                                            rc, 1, NULL, -1, 1);
        }

        Py_END_ALLOW_THREADS;
    }

    if (rc == SQL_NO_DATA_FOUND)
    {
        Py_INCREF(Py_False);
        return Py_False;
    }
    else if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
    {
        _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc, 1,
                                        NULL, -1, 1);
        sprintf(error, "Fetch Failure: %s", IFX_G(__python_stmt_err_msg));
        PyErr_SetString(PyExc_Exception, error);
        return NULL;
    }
    if (rc == SQL_SUCCESS_WITH_INFO)
    {
        _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT,
                                        rc, 1, NULL, -1, 1);
    }
    /* copy the data over return_value */
    if (op & FETCH_ASSOC)
    {
        return_value = PyDict_New();
    }
    else if (op == FETCH_INDEX)
    {
        return_value = PyTuple_New(stmt_res->num_columns);
    }

    for (column_number = 0; column_number < stmt_res->num_columns; column_number++)
    {
        column_type = stmt_res->column_info [column_number].type;
        row_data = &stmt_res->row_data [column_number].data;
        out_length = stmt_res->row_data [column_number].out_length;

        switch (stmt_res->s_case_mode)
        {
        case CASE_LOWER:
            stmt_res->column_info [column_number].name =
                (SQLCHAR*)strtolower((char*)stmt_res->column_info [column_number].name,
                (int)strlen((char*)stmt_res->column_info [column_number].name));
            break;

        case CASE_UPPER:
            stmt_res->column_info [column_number].name =
                (SQLCHAR*)strtoupper((char*)stmt_res->column_info [column_number].name,
                (int)strlen((char*)stmt_res->column_info [column_number].name));
            break;

        case CASE_NATURAL:
        default:
            break;
        }
        if (out_length == SQL_NULL_DATA)
        {
            Py_INCREF(Py_None);
            value = Py_None;
        }
        else
        {
            switch (column_type)
            {
            case SQL_CHAR:
            case SQL_VARCHAR:
                if (stmt_res->s_use_wchar == WCHAR_NO)
                {
                    tmp_length = stmt_res->column_info [column_number].size;
                    value = PyBytes_FromStringAndSize((char *)row_data->str_val, out_length);
                    break;
                }
            case SQL_WCHAR:
            case SQL_WVARCHAR:
                tmp_length = stmt_res->column_info [column_number].size;
                value = getSQLWCharAsPyUnicodeObject(row_data->w_val, out_length);
                break;

            case SQL_LONGVARCHAR:
            case SQL_WLONGVARCHAR:
                tmp_length = out_length;

                wout_ptr = (SQLWCHAR *)ALLOC_N(SQLWCHAR, tmp_length + 1);
                if (wout_ptr == NULL)
                {
                    PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                    return NULL;
                }

                /*  _python_IfxPy_get_data null terminates all output. */
                rc = _python_IfxPy_get_data(stmt_res, column_number + 1, SQL_C_WCHAR, wout_ptr,
                    (tmp_length * sizeof(SQLWCHAR) + 1), &out_length);
                if (rc == SQL_ERROR)
                {
                    return NULL;
                }
                if (out_length == SQL_NULL_DATA)
                {
                    Py_INCREF(Py_None);
                    value = Py_None;
                }
                else
                {
                    value = getSQLWCharAsPyUnicodeObject(wout_ptr, out_length);
                }
                if (wout_ptr != NULL)
                {
                    PyMem_Del(wout_ptr);
                    wout_ptr = NULL;
                }
                break;

            case SQL_DECIMAL:
            case SQL_NUMERIC:
                value = StringOBJ_FromASCII((char *)row_data->str_val);
                break;

            case SQL_TYPE_DATE:
                value = PyDate_FromDate(row_data->date_val->year, row_data->date_val->month, row_data->date_val->day);
                break;

            case SQL_TYPE_TIME:
                value = PyTime_FromTime(row_data->time_val->hour, row_data->time_val->minute, row_data->time_val->second, 0);
                break;

            case SQL_TYPE_TIMESTAMP:
                value = PyDateTime_FromDateAndTime(row_data->ts_val->year, row_data->ts_val->month, row_data->ts_val->day,
                                                   row_data->ts_val->hour, row_data->ts_val->minute, row_data->ts_val->second,
                                                   row_data->ts_val->fraction / 1000);
                break;

            case SQL_INTERVAL_DAY:
            case SQL_INTERVAL_HOUR:
            case SQL_INTERVAL_MINUTE:
            case SQL_INTERVAL_SECOND:
            case SQL_INTERVAL_DAY_TO_HOUR:
            case SQL_INTERVAL_DAY_TO_MINUTE:
            case SQL_INTERVAL_DAY_TO_SECOND:
            case SQL_INTERVAL_HOUR_TO_MINUTE:
            case SQL_INTERVAL_HOUR_TO_SECOND:
            case SQL_INTERVAL_MINUTE_TO_SECOND:
                if (row_data->interval_val->interval_sign == 0) {
                  value = PyDelta_FromDSU((row_data->interval_val->intval.day_second.day * -1) -1,
                                                 (86400 -
                                                  (row_data->interval_val->intval.day_second.hour * 3600) +
                                                  (row_data->interval_val->intval.day_second.minute * 60) +
                                                  row_data->interval_val->intval.day_second.second), 0);
                } else {
                  value = PyDelta_FromDSU(row_data->interval_val->intval.day_second.day,
                                                 ((row_data->interval_val->intval.day_second.hour * 3600) +
                                                  (row_data->interval_val->intval.day_second.minute * 60) +
                                                  row_data->interval_val->intval.day_second.second), 0);
                }
                break;

            case SQL_BIGINT:
                value = PyLong_FromString((char *)row_data->str_val, NULL, 10);
                break;

            case SQL_BIT:
            case SQL_SMALLINT:
                value = PyInt_FromLong(row_data->s_val);
                break;

            case SQL_INTEGER:
                value = PyInt_FromLong(row_data->i_val);
                break;

            case SQL_REAL:
                value = PyFloat_FromDouble(row_data->r_val);
                break;

            case SQL_FLOAT:
                value = PyFloat_FromDouble(row_data->f_val);
                break;

            case SQL_DOUBLE:
                value = PyFloat_FromDouble(row_data->d_val);
                break;

            case SQL_BINARY:
            case SQL_LONGVARBINARY:
            case SQL_VARBINARY:
                if (stmt_res->s_bin_mode == PASSTHRU)
                {
                    value = PyBytes_FromStringAndSize("", 0);
                }
                else
                {
                    if ( stmt_res->column_info [column_number].size < INT_MAX )
                    {
                              value = PyBytes_FromStringAndSize((char *)row_data->str_val, out_length);
                    }
                    else 
                    {
                              tmp_length = out_length;
                              wout_ptr = (SQLPOINTER)ALLOC_N(char, tmp_length + 1);
                              if (wout_ptr == NULL)
                              {
                                  PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                                  return NULL;
                              }
          
                              rc = _python_IfxPy_get_data(stmt_res, column_number + 1, SQL_C_BINARY, wout_ptr,
                                  (tmp_length * sizeof(char) ), &out_length);
                              if (rc == SQL_ERROR)
                              {   
                                  return NULL;
                              }
                              if (out_length == SQL_NULL_DATA)
                              {
                                  Py_INCREF(Py_None);
                                  value = Py_None;
                              }
                              else
                              {
                                  value = PyBytes_FromStringAndSize(wout_ptr, out_length);
                              }
                              if (wout_ptr != NULL)
                              {
                                  PyMem_Del(wout_ptr);
                                  wout_ptr = NULL;
                              }
                    }                 
                }
                break;
           
	   case SQL_INFX_RC_SET:
	   case SQL_INFX_RC_COLLECTION:
	   case SQL_INFX_RC_MULTISET:
	   case SQL_INFX_RC_ROW:
	   case SQL_INFX_RC_LIST:
	   case SQL_INFX_UDT_FIXED:
           case SQL_INFX_UDT_VARYING:
		if (column_type == SQL_INFX_RC_SET || column_type == SQL_INFX_RC_MULTISET 
			|| column_type == SQL_INFX_RC_COLLECTION 
			|| column_type == SQL_INFX_RC_ROW 
			|| column_type == SQL_INFX_UDT_FIXED || column_type == SQL_INFX_UDT_VARYING) {
                        len_terChar = sizeof(SQLCHAR);
                        targetCType = SQL_C_BINARY;
                    } else if (len_terChar == -1) {
                        break;
                    }
                    out_ptr = (void *)ALLOC_N(char, INIT_BUFSIZ + len_terChar);
                    if (out_ptr == NULL) {
                        PyErr_SetString(PyExc_Exception,
                            "Failed to Allocate Memory for Collection/UDT Data");
                        return NULL;
                    }
                    rc = _python_IfxPy_get_data(stmt_res, column_number + 1, targetCType, out_ptr,
               	      INIT_BUFSIZ + len_terChar, &out_length);
                    if (rc == SQL_SUCCESS_WITH_INFO) {
                        void *tmp_out_ptr = NULL;

                        tmp_out_ptr = (void *)ALLOC_N(char, out_length + INIT_BUFSIZ + len_terChar);
                        memcpy(tmp_out_ptr, out_ptr, INIT_BUFSIZ);
                        PyMem_Del(out_ptr);
                        out_ptr = tmp_out_ptr;

                        rc = _python_IfxPy_get_data(stmt_res, column_number + 1, targetCType, (char *)out_ptr + INIT_BUFSIZ,
                            out_length + len_terChar, &out_length);
                        if (rc == SQL_ERROR) {
                            if (out_ptr != NULL) {
                                PyMem_Del(out_ptr);
				out_ptr = NULL;
                            }
                            sprintf(error, "Failed to fetch Collection/UDT  Data: %s",
                                IFX_G(__python_stmt_err_msg));
                            PyErr_SetString(PyExc_Exception, error);
                            return NULL;
                        }


                        if (len_terChar == sizeof(SQLWCHAR)) {
                            value = getSQLWCharAsPyUnicodeObject(out_ptr, INIT_BUFSIZ + out_length);
                        } else {
                            value = PyBytes_FromStringAndSize((char *)row_data->str_val, out_length);
                        }
                    } else if ( rc == SQL_ERROR ) {
                        PyMem_Del(out_ptr);
                        out_ptr = NULL;
                        sprintf(error, "Failed to Collection/UDT  Data: %s",
                            IFX_G(__python_stmt_err_msg));
                        PyErr_SetString(PyExc_Exception, error);
                        return NULL;
                    } else {
                        if (out_length == SQL_NULL_DATA) {
                            Py_INCREF(Py_None);
                            value = Py_None;
                        } else {
                            if (len_terChar == sizeof(SQLWCHAR)) {
                                value =  getSQLWCharAsPyUnicodeObject(out_ptr, out_length);
                            } else {
                                value = PyBytes_FromStringAndSize((char*)out_ptr, out_length);
                            }
                        }
                  }
                    if (out_ptr != NULL) {
                        PyMem_Del(out_ptr);
                        out_ptr = NULL;
                    }
                    break;
            default:
                Py_INCREF(Py_None);
                value = Py_None;
                break;
            }
        }
        if (op & FETCH_ASSOC)
        {
            key = StringOBJ_FromASCII((char*)stmt_res->column_info [column_number].name);
            PyDict_SetItem(return_value, key, value);
            Py_DECREF(key);
        }
        if (op == FETCH_INDEX)
        {
            /* No need to call Py_DECREF as PyTuple_SetItem steals the reference */
            PyTuple_SetItem(return_value, column_number, value);
        }
        else
        {
            if (op == FETCH_BOTH)
            {
                key = PyInt_FromLong(column_number);
                PyDict_SetItem(return_value, key, value);
                Py_DECREF(key);
            }
            Py_DECREF(value);
        }
    }
    if (rc == SQL_SUCCESS_WITH_INFO)
    {
        _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT,
                                        rc, 1, NULL, -1, 1);
    }

    return return_value;
}

/*!# IfxPy.fetch_row
*
* ===Description
* bool IfxPy.fetch_row ( resource stmt [, int row_number] )
*
* Sets the result set pointer to the next row or requested row
*
* Use IfxPy.fetch_row() to iterate through a result set, or to point to a
* specific row in a result set if you requested a scrollable cursor.
*
* To retrieve individual fields from the result set, call the IfxPy.result()
* function. Rather than calling IfxPy.fetch_row() and IfxPy.result(), most
* applications will call one of IfxPy.fetch_assoc(), IfxPy.fetch_both(), or
* IfxPy.fetch_array() to advance the result set pointer and return a complete
* row as an array.
*
* ===Parameters
* ====stmt
*        A valid stmt resource.
*
* ====row_number
*        With scrollable cursors, you can request a specific row number in the
* result set. Row numbering is 1-indexed.
*
* ===Return Values
*
* Returns TRUE if the requested row exists in the result set. Returns FALSE if
* the requested row does not exist in the result set.
*/
static PyObject *IfxPy_fetch_row(PyObject *self, PyObject *args)
{
    PyObject *py_stmt_res = NULL;
    PyObject *py_row_number = NULL;
    SQLINTEGER row_number = -1;
    stmt_handle* stmt_res = NULL;
    int rc;
    char error [DB_MAX_ERR_MSG_LEN];

    if (!PyArg_ParseTuple(args, "O|O", &py_stmt_res, &py_row_number))
        return NULL;

    if (NIL_P(py_stmt_res) || (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType)))
    {
        PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
        return NULL;
    }
    else
    {
        stmt_res = (stmt_handle *)py_stmt_res;
    }

    if (!NIL_P(py_row_number))
    {
        if (PyInt_Check(py_row_number))
        {
            row_number = (SQLINTEGER)PyInt_AsLong(py_row_number);
        }
        else
        {
            PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
            return NULL;
        }
    }
    /* get column header info */
    if (stmt_res->column_info == NULL)
    {
        if (_python_IfxPy_get_result_set_info(stmt_res) < 0)
        {
            sprintf(error, "Column information cannot be retrieved: %s",
                    IFX_G(__python_stmt_err_msg));
            PyErr_SetString(PyExc_Exception, error);
            return NULL;
        }
    }

    /* check if row_number is present */
    if (PyTuple_Size(args) == 2 && row_number > 0)
    {

        rc = SQLFetchScroll((SQLHSTMT)stmt_res->hstmt, SQL_FETCH_ABSOLUTE,
                            row_number);
        if (rc == SQL_SUCCESS_WITH_INFO)
        {
            _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt,
                                            SQL_HANDLE_STMT, rc, 1, NULL,
                                            -1, 1);
        }

    }
    else if (PyTuple_Size(args) == 2 && row_number < 0)
    {
        PyErr_SetString(PyExc_Exception,
                        "Requested row number must be a positive value");
        return NULL;
    }
    else
    {
        /* row_number is NULL or 0; just fetch next row */
        Py_BEGIN_ALLOW_THREADS;
        rc = SQLFetch((SQLHSTMT)stmt_res->hstmt);
        Py_END_ALLOW_THREADS;
    }

    if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO)
    {
        if (rc == SQL_SUCCESS_WITH_INFO)
        {
            _python_IfxPy_check_sql_errors(stmt_res->hstmt,
                                            SQL_HANDLE_STMT, rc, 1,
                                            NULL, -1, 1);
        }
        Py_RETURN_TRUE;
    }
    else if (rc == SQL_NO_DATA_FOUND)
    {
        Py_RETURN_FALSE;
    }
    else
    {
        _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc, 1,
                                        NULL, -1, 1);
        PyErr_Clear();
        Py_RETURN_FALSE;
    }
}

/*!# IfxPy.fetch_assoc
*
* ===Description
* dictionary IfxPy.fetch_assoc ( resource stmt [, int row_number] )
*
* Returns a dictionary, indexed by column name, representing a row in a result  * set.
*
* ===Parameters
* ====stmt
*        A valid stmt resource containing a result set.
*
* ====row_number
*
*        Requests a specific 1-indexed row from the result set. Passing this
* parameter results in a
*        Python warning if the result set uses a forward-only cursor.
*
* ===Return Values
*
* Returns an associative array with column values indexed by the column name
* representing the next
* or requested row in the result set. Returns FALSE if there are no rows left
* in the result set,
* or if the row requested by row_number does not exist in the result set.
*/
static PyObject *IfxPy_fetch_assoc(PyObject *self, PyObject *args)
{
    return _python_IfxPy_bind_fetch_helper(args, FETCH_ASSOC);
}


/*
* IfxPy.fetch_object --    Returns an object with properties representing columns in the fetched row
*
* ===Description
* object IfxPy.fetch_object ( resource stmt [, int row_number] )
*
* Returns an object in which each property represents a column returned in the row fetched from a result set.
*
* ===Parameters
*
* stmt
*        A valid stmt resource containing a result set.
*
* row_number
*        Requests a specific 1-indexed row from the result set. Passing this parameter results in a
*        Python warning if the result set uses a forward-only cursor.
*
* ===Return Values
*
* Returns an object representing a single row in the result set. The properties of the object map
* to the names of the columns in the result set.
*
* The IDS database servers typically fold column names to lower-case,
* so the object properties will reflect that case.
*
* If your SELECT statement calls a scalar function to modify the value of a column, the database servers
* return the column number as the name of the column in the result set. If you prefer a more
* descriptive column name and object property, you can use the AS clause to assign a name
* to the column in the result set.
*
* Returns FALSE if no row was retrieved.
*/
/*
PyObject *IfxPy_fetch_object(int argc, PyObject **argv, PyObject *self)
{
row_hash_struct *row_res;

row_res = ALLOC(row_hash_struct);
row_res->hash = _python_IfxPy_bind_fetch_helper(argc, argv, FETCH_ASSOC);

if (RTEST(row_res->hash)) {
return Data_Wrap_Struct(le_row_struct,
_python_IfxPy_mark_row_struct, _python_IfxPy_free_row_struct,
row_res);
} else {
free(row_res);
return Py_False;
}
}
*/

/*!# IfxPy.fetch_array
*
* ===Description
*
* array IfxPy.fetch_array ( resource stmt [, int row_number] )
*
* Returns a tuple, indexed by column position, representing a row in a result
* set. The columns are 0-indexed.
*
* ===Parameters
*
* ====stmt
*        A valid stmt resource containing a result set.
*
* ====row_number
*        Requests a specific 1-indexed row from the result set. Passing this
* parameter results in a warning if the result set uses a forward-only cursor.
*
* ===Return Values
*
* Returns a 0-indexed tuple with column values indexed by the column position
* representing the next or requested row in the result set. Returns FALSE if
* there are no rows left in the result set, or if the row requested by
* row_number does not exist in the result set.
*/
static PyObject *IfxPy_fetch_array(PyObject *self, PyObject *args)
{
    return _python_IfxPy_bind_fetch_helper(args, FETCH_INDEX);
}

/*!# IfxPy.fetch_both
*
* ===Description
* dictionary IfxPy.fetch_both ( resource stmt [, int row_number] )
*
* Returns a dictionary, indexed by both column name and position, representing  * a row in a result set. Note that the row returned by IfxPy.fetch_both()
* requires more memory than the single-indexed dictionaries/arrays returned by  * IfxPy.fetch_assoc() or IfxPy.fetch_tuple().
*
* ===Parameters
*
* ====stmt
*        A valid stmt resource containing a result set.
*
* ====row_number
*        Requests a specific 1-indexed row from the result set. Passing this
* parameter results in a warning if the result set uses a forward-only cursor.
*
* ===Return Values
*
* Returns a dictionary with column values indexed by both the column name and
* 0-indexed column number.
* The dictionary represents the next or requested row in the result set.
* Returns FALSE if there are no rows left in the result set, or if the row
* requested by row_number does not exist in the result set.
*/
static PyObject *IfxPy_fetch_both(PyObject *self, PyObject *args)
{
    return _python_IfxPy_bind_fetch_helper(args, FETCH_BOTH);
}

/*!# IfxPy.set_option
*
* ===Description
* bool IfxPy.set_option ( resource resc, array options, int type )
*
* Sets options for a connection or statement resource. You cannot set options
* for result set resources.
*
* ===Parameters
*
* ====resc
*        A valid connection or statement resource.
*
* ====options
*        The options to be set
*
* ====type
*        A field that specifies the resource type (1 = Connection,
* NON-1 = Statement)
*
* ===Return Values
*
* Returns TRUE on success or FALSE on failure
*/
static PyObject *IfxPy_set_option(PyObject *self, PyObject *args)
{
    PyObject *conn_or_stmt = NULL;
    PyObject *options = NULL;
    PyObject *py_type = NULL;
    stmt_handle *stmt_res = NULL;
    conn_handle *conn_res;
    int rc = 0;
    long type = 0;

    if (!PyArg_ParseTuple(args, "OOO", &conn_or_stmt, &options, &py_type))
        return NULL;

    if (!NIL_P(conn_or_stmt))
    {
        if (!NIL_P(py_type))
        {
            if (PyInt_Check(py_type))
            {
                type = (int)PyInt_AsLong(py_type);
            }
            else
            {
                PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
                return NULL;
            }
        }
        if (type == 1)
        {
            if (!PyObject_TypeCheck(conn_or_stmt, &conn_handleType))
            {
                PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
                return NULL;
            }
            conn_res = (conn_handle *)conn_or_stmt;

            if (!NIL_P(options))
            {
                rc = _python_IfxPy_parse_options(options, SQL_HANDLE_DBC,
                                                  conn_res);
                if (rc == SQL_ERROR)
                {
                    PyErr_SetString(PyExc_Exception,
                                    "Options Array must have string indexes");
                    return NULL;
                }
            }
        }
        else
        {
            if (!PyObject_TypeCheck(conn_or_stmt, &stmt_handleType))
            {
                PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
                return NULL;
            }
            stmt_res = (stmt_handle *)conn_or_stmt;

            if (!NIL_P(options))
            {
                rc = _python_IfxPy_parse_options(options, SQL_HANDLE_STMT,
                                                  stmt_res);
                if (rc == SQL_ERROR)
                {
                    PyErr_SetString(PyExc_Exception,
                                    "Options Array must have string indexes");
                    return NULL;
                }
            }
        }
        Py_INCREF(Py_True);
        return Py_True;
    }
    else
    {
        Py_INCREF(Py_False);
        return Py_False;
    }
}

static PyObject *IfxPy_get_db_info(PyObject *self, PyObject *args)
{
    PyObject *py_conn_res = NULL;
    PyObject *return_value = NULL;
    PyObject *py_option = NULL;
    SQLINTEGER option = 0;
    conn_handle *conn_res;
    int rc = 0;
    SQLCHAR *value = NULL;

    if (!PyArg_ParseTuple(args, "OO", &py_conn_res, &py_option))
        return NULL;

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }
        if (!NIL_P(py_option))
        {
            if (PyInt_Check(py_option))
            {
                option = (SQLINTEGER)PyInt_AsLong(py_option);
            }
            else
            {
                PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
                return NULL;
            }
        }
        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            return NULL;
        }

        value = (SQLCHAR*)ALLOC_N(char, ACCTSTR_LEN + 1);

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, (SQLSMALLINT)option, (SQLPOINTER)value,
                        ACCTSTR_LEN, NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            if (value != NULL)
            {
                PyMem_Del(value);
                value = NULL;
            }
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value = StringOBJ_FromASCII((char *)value);
            if (value != NULL)
            {
                PyMem_Del(value);
                value = NULL;
            }
            return return_value;
        }
    }
    Py_INCREF(Py_False);
    return Py_False;
}

/*!# IfxPy.server_info
*
* ===Description
* object IfxPy.server_info ( resource connection )
*
* This function returns a read-only object with information about the IDS
* or Informix Dynamic Server.
* The following table lists the database server properties:
*
* ====Table 1. Database server properties
* Property name:: Description (Return type)
*
* DBMS_NAME:: The name of the database server to which you are connected. For
* DB2 servers this is a combination of DB2 followed by the operating system on
* which the database server is running. (string)
*
* DBMS_VER:: The version of the database server, in the form of a string
* "MM.mm.uuuu" where MM is the major version, mm is the minor version, and
* uuuu is the update. For example, "08.02.0001" represents major version 8,
* minor version 2, update 1. (string)
*
* DB_CODEPAGE:: The code page of the database to which you are connected. (int)
*
* DB_NAME:: The name of the database to which you are connected. (string)
*
* DFT_ISOLATION:: The default transaction isolation level supported by the
* server: (string)
*
*                         UR:: Uncommitted read: changes are immediately
* visible by all concurrent transactions.
*
*                         CS:: Cursor stability: a row read by one transaction
* can be altered and committed by a second concurrent transaction.
*
*                         RS:: Read stability: a transaction can add or remove
* rows matching a search condition or a pending transaction.
*
*                         RR:: Repeatable read: data affected by pending
* transaction is not available to other transactions.
*
*                         NC:: No commit: any changes are visible at the end of
* a successful operation. Explicit commits and rollbacks are not allowed.
*
* IDENTIFIER_QUOTE_CHAR:: The character used to delimit an identifier. (string)
*
* INST_NAME:: The instance on the database server that contains the database.
* (string)
*
* ISOLATION_OPTION:: An array of the isolation options supported by the
* database server. The isolation options are described in the DFT_ISOLATION
* property. (array)
*
* KEYWORDS:: An array of the keywords reserved by the database server. (array)
*
* LIKE_ESCAPE_CLAUSE:: TRUE if the database server supports the use of % and _
* wildcard characters. FALSE if the database server does not support these
* wildcard characters. (bool)
*
* MAX_COL_NAME_LEN:: Maximum length of a column name supported by the database
* server, expressed in bytes. (int)
*
* MAX_IDENTIFIER_LEN:: Maximum length of an SQL identifier supported by the
* database server, expressed in characters. (int)
*
* MAX_INDEX_SIZE:: Maximum size of columns combined in an index supported by
* the database server, expressed in bytes. (int)
*
* MAX_PROC_NAME_LEN:: Maximum length of a procedure name supported by the
* database server, expressed in bytes. (int)
*
* MAX_ROW_SIZE:: Maximum length of a row in a base table supported by the
* database server, expressed in bytes. (int)
*
* MAX_SCHEMA_NAME_LEN:: Maximum length of a schema name supported by the
* database server, expressed in bytes. (int)
*
* MAX_STATEMENT_LEN:: Maximum length of an SQL statement supported by the
* database server, expressed in bytes. (int)
*
* MAX_TABLE_NAME_LEN:: Maximum length of a table name supported by the
* database server, expressed in bytes. (bool)
*
* NON_NULLABLE_COLUMNS:: TRUE if the database server supports columns that can
* be defined as NOT NULL, FALSE if the database server does not support columns
* defined as NOT NULL. (bool)
*
* PROCEDURES:: TRUE if the database server supports the use of the CALL
* statement to call stored procedures, FALSE if the database server does not
* support the CALL statement. (bool)
*
* SPECIAL_CHARS:: A string containing all of the characters other than a-Z,
* 0-9, and underscore that can be used in an identifier name. (string)
*
* SQL_CONFORMANCE:: The level of conformance to the ANSI/ISO SQL-92
* specification offered by the database server: (string)
*
*                            ENTRY:: Entry-level SQL-92 compliance.
*
*                            FIPS127:: FIPS-127-2 transitional compliance.
*
*                            FULL:: Full level SQL-92 compliance.
*
*                            INTERMEDIATE:: Intermediate level SQL-92
*                                            compliance.
*
* ===Parameters
*
* ====connection
*        Specifies an active DB2 client connection.
*
* ===Return Values
*
* Returns an object on a successful call. Returns FALSE on failure.
*/
static PyObject *IfxPy_server_info(PyObject *self, PyObject *args)
{
    PyObject *py_conn_res = NULL;
    conn_handle *conn_res;
    int rc = 0;
    char buffer11 [11];
    char buffer255 [255];
    char buffer2k [2048];
    SQLSMALLINT bufferint16;
    SQLUINTEGER bufferint32;
    SQLINTEGER bitmask;
    char *keyword;
    char *last;
    PyObject *karray;
    int numkw = 0;
    int count = 0;
    PyObject *array;
    PyObject *rv = NULL;

    le_server_info *return_value = PyObject_NEW(le_server_info,
                                                &server_infoType);

    if (!PyArg_ParseTuple(args, "O", &py_conn_res))
        return NULL;

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }

        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            return NULL;
        }

        /* DBMS_NAME */
        memset(buffer255, 0, sizeof(buffer255));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_DBMS_NAME, (SQLPOINTER)buffer255,
                        sizeof(buffer255), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            return_value->DBMS_NAME = StringOBJ_FromASCII(buffer255);
        }

        /* DBMS_VER */
        memset(buffer11, 0, sizeof(buffer11));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_DBMS_VER, (SQLPOINTER)buffer11,
                        sizeof(buffer11), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->DBMS_VER = StringOBJ_FromASCII(buffer11);
        }

        /* DB_CODEPAGE */
        //bufferint32 = 0;

        //Py_BEGIN_ALLOW_THREADS;
        //rc = SQLGetInfo(conn_res->hdbc, SQL_DATABASE_CODEPAGE, &bufferint32,
        //                sizeof(bufferint32), NULL);
        //Py_END_ALLOW_THREADS;

        //if (rc == SQL_ERROR)
        //{
        //    _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
        //                                    NULL, -1, 1);
        //    PyErr_Clear();
        //    Py_RETURN_FALSE;
        //}
        //else
        //{
        //    if (rc == SQL_SUCCESS_WITH_INFO)
        //    {
        //        _python_IfxPy_check_sql_errors(conn_res->hdbc,
        //                                        SQL_HANDLE_DBC, rc, 1,
        //                                        NULL, -1, 1);
        //    }
        //    return_value->DB_CODEPAGE = PyInt_FromLong(bufferint32);
        //}


        /* DB_NAME */
        memset(buffer255, 0, sizeof(buffer255));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_DATABASE_NAME, (SQLPOINTER)buffer255,
                        sizeof(buffer255), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            Py_INCREF(Py_False);
            return Py_False;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->DB_NAME = StringOBJ_FromASCII(buffer255);
        }


        /* INST_NAME */
        memset(buffer255, 0, sizeof(buffer255));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_SERVER_NAME, (SQLPOINTER)buffer255,
                        sizeof(buffer255), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->INST_NAME = StringOBJ_FromASCII(buffer255);
        }

        /* SPECIAL_CHARS */
        memset(buffer255, 0, sizeof(buffer255));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_SPECIAL_CHARACTERS,
            (SQLPOINTER)buffer255, sizeof(buffer255), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->SPECIAL_CHARS = StringOBJ_FromASCII(buffer255);
        }


        /* KEYWORDS */
        memset(buffer2k, 0, sizeof(buffer2k));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_KEYWORDS, (SQLPOINTER)buffer2k,
                        sizeof(buffer2k), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }

            for (last = buffer2k; *last; last++)
            {
                if (*last == ',')
                {
                    numkw++;
                }
            }
            karray = PyTuple_New(numkw + 1);

            for (keyword = last = buffer2k; *last; last++)
            {
                if (*last == ',')
                {
                    *last = '\0';
                    PyTuple_SetItem(karray, count, StringOBJ_FromASCII(keyword));
                    keyword = last + 1;
                    count++;
                }
            }
            if (*keyword)
                PyTuple_SetItem(karray, count, StringOBJ_FromASCII(keyword));
            return_value->KEYWORDS = karray;
        }

        /* DFT_ISOLATION */
        bitmask = 0;
        memset(buffer11, 0, sizeof(buffer11));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_DEFAULT_TXN_ISOLATION, &bitmask,
                        sizeof(bitmask), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            if (bitmask & SQL_TXN_READ_UNCOMMITTED)
            {
                strcpy((char *)buffer11, "UR");
            }
            if (bitmask & SQL_TXN_READ_COMMITTED)
            {
                strcpy((char *)buffer11, "CS");
            }
            if (bitmask & SQL_TXN_REPEATABLE_READ)
            {
                strcpy((char *)buffer11, "RS");
            }
            if (bitmask & SQL_TXN_SERIALIZABLE)
            {
                strcpy((char *)buffer11, "RR");
            }
            return_value->DFT_ISOLATION = StringOBJ_FromASCII(buffer11);
        }


        /* ISOLATION_OPTION */
        bitmask = 0;
        count = 0;
        memset(buffer11, 0, sizeof(buffer11));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_TXN_ISOLATION_OPTION, &bitmask,
                        sizeof(bitmask), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }

            array = PyTuple_New(5);

            if (bitmask & SQL_TXN_READ_UNCOMMITTED)
            {
                PyTuple_SetItem(array, count, StringOBJ_FromASCII("UR"));
                count++;
            }
            if (bitmask & SQL_TXN_READ_COMMITTED)
            {
                PyTuple_SetItem(array, count, StringOBJ_FromASCII("CS"));
                count++;
            }
            if (bitmask & SQL_TXN_REPEATABLE_READ)
            {
                PyTuple_SetItem(array, count, StringOBJ_FromASCII("RS"));
                count++;
            }
            if (bitmask & SQL_TXN_SERIALIZABLE)
            {
                PyTuple_SetItem(array, count, StringOBJ_FromASCII("RR"));
                count++;
            }
            _PyTuple_Resize(&array, count);

            return_value->ISOLATION_OPTION = array;
        }


        /* SQL_CONFORMANCE */
        bufferint32 = 0;
        memset(buffer255, 0, sizeof(buffer255));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_ODBC_SQL_CONFORMANCE, &bufferint32,
                        sizeof(bufferint32), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            switch (bufferint32)
            {
            case SQL_SC_SQL92_ENTRY:
                strcpy((char *)buffer255, "ENTRY");
                break;
            case SQL_SC_FIPS127_2_TRANSITIONAL:
                strcpy((char *)buffer255, "FIPS127");
                break;
            case SQL_SC_SQL92_FULL:
                strcpy((char *)buffer255, "FULL");
                break;
            case SQL_SC_SQL92_INTERMEDIATE:
                strcpy((char *)buffer255, "INTERMEDIATE");
                break;
            default:
                break;
            }
            return_value->SQL_CONFORMANCE = StringOBJ_FromASCII(buffer255);
        }

        /* PROCEDURES */
        memset(buffer11, 0, sizeof(buffer11));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_PROCEDURES, (SQLPOINTER)buffer11,
                        sizeof(buffer11), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            if (strcmp((char *)buffer11, "Y") == 0)
            {
                Py_INCREF(Py_True);
                return_value->PROCEDURES = Py_True;
            }
            else
            {
                Py_INCREF(Py_False);
                return_value->PROCEDURES = Py_False;
            }
        }

        /* IDENTIFIER_QUOTE_CHAR */
        memset(buffer11, 0, sizeof(buffer11));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_IDENTIFIER_QUOTE_CHAR,
            (SQLPOINTER)buffer11, sizeof(buffer11), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->IDENTIFIER_QUOTE_CHAR = StringOBJ_FromASCII(buffer11);
        }

        /* LIKE_ESCAPE_CLAUSE */
        memset(buffer11, 0, sizeof(buffer11));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_LIKE_ESCAPE_CLAUSE,
            (SQLPOINTER)buffer11, sizeof(buffer11), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            if (strcmp(buffer11, "Y") == 0)
            {
                Py_INCREF(Py_True);
                return_value->LIKE_ESCAPE_CLAUSE = Py_True;
            }
            else
            {
                Py_INCREF(Py_False);
                return_value->LIKE_ESCAPE_CLAUSE = Py_False;
            }
        }

        /* MAX_COL_NAME_LEN */
        bufferint16 = 0;

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_MAX_COLUMN_NAME_LEN, &bufferint16,
                        sizeof(bufferint16), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->MAX_COL_NAME_LEN = PyInt_FromLong(bufferint16);
        }

        /* MAX_ROW_SIZE */
        bufferint32 = 0;

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_MAX_ROW_SIZE, &bufferint32,
                        sizeof(bufferint32), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->MAX_ROW_SIZE = PyInt_FromLong(bufferint32);
        }


        /* MAX_IDENTIFIER_LEN */
        bufferint16 = 0;

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_MAX_IDENTIFIER_LEN, &bufferint16,
                        sizeof(bufferint16), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->MAX_IDENTIFIER_LEN = PyInt_FromLong(bufferint16);
        }

        /* MAX_INDEX_SIZE */
        bufferint32 = 0;

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_MAX_INDEX_SIZE, &bufferint32,
                        sizeof(bufferint32), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->MAX_INDEX_SIZE = PyInt_FromLong(bufferint32);
        }

        /* MAX_PROC_NAME_LEN */
        bufferint16 = 0;

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_MAX_PROCEDURE_NAME_LEN, &bufferint16,
                        sizeof(bufferint16), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->MAX_PROC_NAME_LEN = PyInt_FromLong(bufferint16);
        }


        /* MAX_SCHEMA_NAME_LEN */
        bufferint16 = 0;

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_MAX_SCHEMA_NAME_LEN, &bufferint16,
                        sizeof(bufferint16), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->MAX_SCHEMA_NAME_LEN = PyInt_FromLong(bufferint16);
        }

        /* MAX_STATEMENT_LEN */
        bufferint32 = 0;

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_MAX_STATEMENT_LEN, &bufferint32,
                        sizeof(bufferint32), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->MAX_STATEMENT_LEN = PyInt_FromLong(bufferint32);
        }

        /* MAX_TABLE_NAME_LEN */
        bufferint16 = 0;

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_MAX_TABLE_NAME_LEN, &bufferint16,
                        sizeof(bufferint16), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->MAX_TABLE_NAME_LEN = PyInt_FromLong(bufferint16);
        }

        /* NON_NULLABLE_COLUMNS */
        bufferint16 = 0;

        Py_BEGIN_ALLOW_THREADS;

        rc = SQLGetInfo(conn_res->hdbc, SQL_NON_NULLABLE_COLUMNS, &bufferint16,
                        sizeof(bufferint16), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            switch (bufferint16)
            {
            case SQL_NNC_NON_NULL:
                Py_INCREF(Py_True);
                rv = Py_True;
                break;
            case SQL_NNC_NULL:
                Py_INCREF(Py_False);
                rv = Py_False;
                break;
            default:
                break;
            }
            return_value->NON_NULLABLE_COLUMNS = rv;
        }
        return (PyObject *)return_value;
    }
    Py_RETURN_FALSE;
}

/*!# IfxPy.client_info
*
* ===Description
* object IfxPy.client_info ( resource connection )
*
* This function returns a read-only object with information about the IDS client.
The following table lists the client properties:
*
* ====IBM Data Server client properties
*
* APPL_CODEPAGE:: The application code page.
*
* CONN_CODEPAGE:: The code page for the current connection.
*
* DATA_SOURCE_NAME:: The data source name (DSN) used to create the current
* connection to the database.
*
* DRIVER_NAME:: The name of the library that implements the Call Level
* Interface (CLI) specification.
*
* DRIVER_ODBC_VER:: The version of ODBC that the IBM Data Server client
* supports. This returns a string "MM.mm" where MM is the major version and mm
* is the minor version. The IBM Data Server client always returns "03.51".
*
* DRIVER_VER:: The version of the client, in the form of a string "MM.mm.uuuu"
* where MM is the major version, mm is the minor version, and uuuu is the
* update. For example, "08.02.0001" represents major version 8, minor version
* 2, update 1. (string)
*
* ODBC_SQL_CONFORMANCE:: There are three levels of ODBC SQL grammar supported
* by the client: MINIMAL (Supports the minimum ODBC SQL grammar), CORE
* (Supports the core ODBC SQL grammar), EXTENDED (Supports extended ODBC SQL
* grammar).
*
* ODBC_VER:: The version of ODBC that the ODBC driver manager supports. This
* returns a string "MM.mm.rrrr" where MM is the major version, mm is the minor
* version, and rrrr is the release. The client always returns "03.01.0000".
*
* ===Parameters
*
* ====connection
*
*      Specifies an active IBM Data Server client connection.
*
* ===Return Values
*
* Returns an object on a successful call. Returns FALSE on failure.
*/
static PyObject *IfxPy_client_info(PyObject *self, PyObject *args)
{
    PyObject *py_conn_res = NULL;
    conn_handle *conn_res = NULL;
    int rc = 0;
    char buffer255 [255];
    SQLSMALLINT bufferint16;

    le_client_info *return_value = PyObject_NEW(le_client_info,
                                                &client_infoType);

    if (!PyArg_ParseTuple(args, "O", &py_conn_res))
        return NULL;

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }
        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            return NULL;
        }

        /* DRIVER_NAME */
        memset(buffer255, 0, sizeof(buffer255));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_DRIVER_NAME, (SQLPOINTER)buffer255,
                        sizeof(buffer255), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->DRIVER_NAME = StringOBJ_FromASCII(buffer255);
        }

        /* DRIVER_VER */
        memset(buffer255, 0, sizeof(buffer255));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_DRIVER_VER, (SQLPOINTER)buffer255,
                        sizeof(buffer255), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->DRIVER_VER = StringOBJ_FromASCII(buffer255);
        }

        /* DATA_SOURCE_NAME */
        memset(buffer255, 0, sizeof(buffer255));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_DATA_SOURCE_NAME,
            (SQLPOINTER)buffer255, sizeof(buffer255), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->DATA_SOURCE_NAME = StringOBJ_FromASCII(buffer255);
        }

        /* DRIVER_ODBC_VER */
        memset(buffer255, 0, sizeof(buffer255));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_DRIVER_ODBC_VER,
            (SQLPOINTER)buffer255, sizeof(buffer255), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->DRIVER_ODBC_VER = StringOBJ_FromASCII(buffer255);
        }


        /* ODBC_VER */
        memset(buffer255, 0, sizeof(buffer255));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_ODBC_VER, (SQLPOINTER)buffer255,
                        sizeof(buffer255), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            return_value->ODBC_VER = StringOBJ_FromASCII(buffer255);
        }


        /* ODBC_SQL_CONFORMANCE */
        bufferint16 = 0;
        memset(buffer255, 0, sizeof(buffer255));

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetInfo(conn_res->hdbc, SQL_ODBC_SQL_CONFORMANCE, &bufferint16,
                        sizeof(bufferint16), NULL);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
            Py_RETURN_FALSE;
        }
        else
        {
            if (rc == SQL_SUCCESS_WITH_INFO)
            {
                _python_IfxPy_check_sql_errors(conn_res->hdbc,
                                                SQL_HANDLE_DBC, rc, 1,
                                                NULL, -1, 1);
            }
            switch (bufferint16)
            {
            case SQL_OSC_MINIMUM:
                strcpy((char *)buffer255, "MINIMUM");
                break;
            case SQL_OSC_CORE:
                strcpy((char *)buffer255, "CORE");
                break;
            case SQL_OSC_EXTENDED:
                strcpy((char *)buffer255, "EXTENDED");
                break;
            default:
                break;
            }
            return_value->ODBC_SQL_CONFORMANCE = StringOBJ_FromASCII(buffer255);
        }


        /* APPL_CODEPAGE */
        //bufferint32 = 0;

        //Py_BEGIN_ALLOW_THREADS;
        //rc = SQLGetInfo(conn_res->hdbc, SQL_APPLICATION_CODEPAGE, &bufferint32,
        //                sizeof(bufferint32), NULL);
        //Py_END_ALLOW_THREADS;

        //if (rc == SQL_ERROR)
        //{
        //    _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
        //                                    NULL, -1, 1);
        //    PyErr_Clear();
        //    Py_RETURN_FALSE;
        //}
        //else
        //{
        //    if (rc == SQL_SUCCESS_WITH_INFO)
        //    {
        //        _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
        //                                        NULL, -1, 1);
        //    }
        //    return_value->APPL_CODEPAGE = PyInt_FromLong(bufferint32);
        //}

        /* CONN_CODEPAGE */
        //bufferint32 = 0;

        //Py_BEGIN_ALLOW_THREADS;
        //rc = SQLGetInfo(conn_res->hdbc, SQL_CONNECT_CODEPAGE, &bufferint32,
        //                sizeof(bufferint32), NULL);
        //Py_END_ALLOW_THREADS;

        //if (rc == SQL_ERROR)
        //{
        //    _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
        //                                    NULL, -1, 1);
        //    PyErr_Clear();
        //    Py_RETURN_FALSE;
        //}
        //else
        //{
        //    if (rc == SQL_SUCCESS_WITH_INFO)
        //    {
        //        _python_IfxPy_check_sql_errors(conn_res->hdbc,
        //                                        SQL_HANDLE_DBC, rc, 1,
        //                                        NULL, -1, 1);
        //    }
        //    return_value->CONN_CODEPAGE = PyInt_FromLong(bufferint32);
        //}


        return (PyObject *)return_value;
    }
    PyErr_Clear();
    Py_RETURN_FALSE;
}

/*!# IfxPy.active
*
* ===Description
* Py_True/Py_False IfxPy.active(resource connection)
*
* Checks if the specified connection resource is active
*
* Returns Py_True if the given connection resource is active
*
* ===Parameters
* ====connection
*        The connection resource to be validated.
*
* ===Return Values
*
* Returns Py_True if the given connection resource is active, otherwise it will
* return Py_False
*/
static PyObject *IfxPy_active(PyObject *self, PyObject *args)
{
    PyObject *py_conn_res = NULL;
    int rc;
    conn_handle *conn_res = NULL;
    SQLINTEGER conn_dead;

    conn_dead = 1;

    if (!PyArg_ParseTuple(args, "O", &py_conn_res))
        return NULL;

    if (!(NIL_P(py_conn_res) || (py_conn_res == Py_None)))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }
        rc = SQLGetConnectAttr(conn_res->hdbc, SQL_ATTR_CONNECTION_DEAD,
            (SQLPOINTER)&conn_dead, 0, NULL);
        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC, rc, 1,
                                            NULL, -1, 1);
            PyErr_Clear();
        }
        if (conn_dead == 0)
        {
            Py_RETURN_TRUE;
        }
    }
    Py_RETURN_FALSE; 
}

/*!# IfxPy.get_option
*
* ===Description
* mixed IfxPy.get_option ( resource resc, int options, int type )
*
* Returns a value, that is the current setting of a connection or statement
* attribute.
*
* ===Parameters
*
* ====resc
*        A valid connection or statement resource containing a result set.
*
* ====options
*        The options to be retrieved
*
* ====type
*        A field that specifies the resource type (1 = Connection,
*        non - 1 = Statement)
*
* ===Return Values
*
* Returns the current setting of the resource attribute provided.
*/
static PyObject *IfxPy_get_option(PyObject *self, PyObject *args)
{
    PyObject *conn_or_stmt = NULL;
    PyObject *retVal = NULL;
    PyObject *py_op_integer = NULL;
    PyObject *py_type = NULL;
    SQLCHAR *value = NULL;
    SQLINTEGER value_int = 0;
    conn_handle *conn_res = NULL;
    stmt_handle *stmt_res = NULL;
    SQLINTEGER op_integer = 0;
    long type = 0;
    int rc;

    if (!PyArg_ParseTuple(args, "OOO", &conn_or_stmt, &py_op_integer, &py_type))
        return NULL;

    if (!NIL_P(conn_or_stmt))
    {
        if (!NIL_P(py_op_integer))
        {
            if (PyInt_Check(py_op_integer))
            {
                op_integer = (SQLINTEGER)PyInt_AsLong(py_op_integer);
            }
            else
            {
                PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
                return NULL;
            }
        }
        if (!NIL_P(py_type))
        {
            if (PyInt_Check(py_type))
            {
                type = PyInt_AsLong(py_type);
            }
            else
            {
                PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
                return NULL;
            }
        }
        // Checking to see if we are getting a connection option (1) or a
        // statement option (non - 1)

        if (type == 1)
        {
            if (!PyObject_TypeCheck(conn_or_stmt, &conn_handleType))
            {
                PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
                return NULL;
            }
            conn_res = (conn_handle *)conn_or_stmt;

            /* Check to ensure the connection resource given is active */
            if (!conn_res->handle_active)
            {
                PyErr_SetString(PyExc_Exception, "Connection is not active");
                return NULL;
            }
            /* Check that the option given is not null */
            if (!NIL_P(py_op_integer))
            {
                // ACCTSTR_LEN is the largest possible length of the options to retrieve
                value = (SQLCHAR*)ALLOC_N(char, ACCTSTR_LEN + 1);
                if (value == NULL)
                {
                    PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                    return NULL;
                }

                rc = SQLGetConnectAttr((SQLHDBC)conn_res->hdbc, op_integer,
                    (SQLPOINTER)value, ACCTSTR_LEN, NULL);
                if (rc == SQL_ERROR)
                {
                    _python_IfxPy_check_sql_errors(conn_res->hdbc, SQL_HANDLE_DBC,
                                                    rc, 1, NULL, -1, 1);
                    if (value != NULL)
                    {
                        PyMem_Del(value);
                        value = NULL;
                    }
                    PyErr_Clear();
                    Py_RETURN_FALSE;
                }
                retVal = StringOBJ_FromASCII((char *)value);
                if (value != NULL)
                {
                    PyMem_Del(value);
                    value = NULL;
                }
                return retVal;
            }
            else
            {
                PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
                return NULL;
            }
            /* At this point we know we are to retreive a statement option */
        }
        else
        {
            stmt_res = (stmt_handle *)conn_or_stmt;

            /* Check that the option given is not null */
            if (!NIL_P(py_op_integer))
            {
                // Checking that the option to get is the cursor type because that
                // is what we support here
                if (op_integer == SQL_ATTR_CURSOR_TYPE)
                {
                    rc = SQLGetStmtAttr((SQLHSTMT)stmt_res->hstmt, op_integer,
                                        &value_int, SQL_IS_INTEGER, NULL);
                    if (rc == SQL_ERROR)
                    {
                        _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
                        PyErr_Clear();
                        Py_RETURN_FALSE;
                    }
                    return PyInt_FromLong(value_int);
                }
                else
                {
                    PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
                    return NULL;
                }
            }
            else
            {
                PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
                return NULL;
            }
        }
    }
    PyErr_Clear();
    Py_RETURN_FALSE;
}


static void _build_client_err_list(error_msg_node *head_error_list, char *err_msg)
{
    error_msg_node *tmp_err = NULL, *curr_err = head_error_list->next, *prv_err = NULL;
    tmp_err = ALLOC(error_msg_node);
    memset(tmp_err, 0, sizeof(error_msg_node));
    strcpy(tmp_err->err_msg, err_msg);
    tmp_err->next = NULL;
    while (curr_err != NULL)
    {
        prv_err = curr_err;
        curr_err = curr_err->next;
    }

    if (head_error_list->next == NULL)
    {
        head_error_list->next = tmp_err;
    }
    else
    {
        prv_err->next = tmp_err;
    }
}




//IfxPy.execute_many -- can be used to execute an SQL with multiple values of parameter marker.
//===Description
//int IfxPy.execute_many(IFXStatement, Parameters[, Options])
//Returns number of inserted/updated/deleted rows if batch executed successfully.
//return NULL if batch fully or partialy fails  (All the rows executed except for which error occurs).

static PyObject* IfxPy_execute_many(PyObject *self, PyObject *args)
{
    PyObject *options = NULL;
    PyObject *params = NULL;
    PyObject *py_stmt_res = NULL;
    stmt_handle *stmt_res = NULL;
    char error [DB_MAX_ERR_MSG_LEN];
    PyObject *data = NULL;
    error_msg_node *head_error_list = NULL;
    int err_count = 0;

    int rc;
    int i = 0;
    SQLSMALLINT numOpts = 0;
    int numOfRows = 0;
    int numOfParam = 0;
    SQLINTEGER row_cnt = 0;
    int chaining_start = 0;

    SQLSMALLINT *data_type;
    SQLUINTEGER precision;
    SQLSMALLINT scale;
    SQLSMALLINT nullable;
    SQLSMALLINT *ref_data_type;

     // Get the parameters
     //     1. statement handler Object
     //     2. Parameters
     //     3. Options (optional) 
    if (!PyArg_ParseTuple(args, "OO|O", &py_stmt_res, &params, &options))
        return NULL;

    if (!NIL_P(py_stmt_res))
    {
        if (!PyObject_TypeCheck(py_stmt_res, &stmt_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
            return NULL;
        }
        else
        {
            stmt_res = (stmt_handle *)py_stmt_res;
        }
        // Free any cursors that might have been allocated in a previous call to SQLExecute 
        Py_BEGIN_ALLOW_THREADS;
        SQLFreeStmt((SQLHSTMT)stmt_res->hstmt, SQL_CLOSE);
        Py_END_ALLOW_THREADS;

        _python_IfxPy_clear_stmt_err_cache();
        stmt_res->head_cache_list = NULL;
        stmt_res->current_node = NULL;

        // Bind parameters 
        Py_BEGIN_ALLOW_THREADS;
        rc = SQLNumParams((SQLHSTMT)stmt_res->hstmt, (SQLSMALLINT*)&numOpts);
        Py_END_ALLOW_THREADS;

        data_type = (SQLSMALLINT*)ALLOC_N(SQLSMALLINT, numOpts);
        ref_data_type = (SQLSMALLINT*)ALLOC_N(SQLSMALLINT, numOpts);
        for (i = 0; i < numOpts; i++)
        {
            ref_data_type [i] = -1;
        }
        if (numOpts != 0)
        {
            for (i = 0; i < numOpts; i++)
            {
                Py_BEGIN_ALLOW_THREADS;
                rc = SQLDescribeParam((SQLHSTMT)stmt_res->hstmt, i + 1,
                    (SQLSMALLINT*)(data_type + i), &precision, (SQLSMALLINT*)&scale,
                                      (SQLSMALLINT*)&nullable);
                Py_END_ALLOW_THREADS;

                if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
                {
                    _python_IfxPy_check_sql_errors(stmt_res->hstmt,
                                                    SQL_HANDLE_STMT,
                                                    rc, 1, NULL, -1, 1);
                }
                if (rc == SQL_ERROR)
                {
                    PyErr_SetString(PyExc_Exception, IFX_G(__python_stmt_err_msg));
                    return NULL;
                }
                build_list(stmt_res, i + 1, data_type [i], precision,
                           scale, nullable);
            }
        }

        // Execute SQL for all set of parameters 
        numOfRows = PyTuple_Size(params);
        head_error_list = ALLOC(error_msg_node);
        memset(head_error_list, 0, sizeof(error_msg_node));
        head_error_list->next = NULL;
        if (numOfRows > 0)
        {
            for (i = 0; i < numOfRows; i++)
            {
                int j = 0;
                param_node *curr = NULL;
                PyObject *param = PyTuple_GET_ITEM(params, i);
                error [0] = '\0';
                if (!PyTuple_Check(param))
                {
                    sprintf(error, "Value parameter: %d is not a tuple", i + 1);
                    _build_client_err_list(head_error_list, error);
                    err_count++;
                    continue;
                }

                numOfParam = PyTuple_Size(param);
                if (numOpts < numOfParam)
                {
                    // More are passed in -- Warning - Use the max number present 
                    sprintf(error, "Value parameter tuple: %d has more no of param", i + 1);
                    _build_client_err_list(head_error_list, error);
                    err_count++;
                    continue;
                }
                else if (numOpts > numOfParam)
                {
                    // If there are less params passed in, than are present
                    // -- Error
                    
                    sprintf(error, "Value parameter tuple: %d has less no of param", i + 1);
                    _build_client_err_list(head_error_list, error);
                    err_count++;
                    continue;
                }

                // Bind values from the parameters_tuple to params 
                curr = stmt_res->head_cache_list;

                while (curr != NULL)
                {
                    data = PyTuple_GET_ITEM(param, j);
                    if (data == NULL)
                    {
                        sprintf(error, "NULL value passed for value parameter: %d", i + 1);
                        _build_client_err_list(head_error_list, error);
                        err_count++;
                        break;
                    }

                    //if (chaining_start)
                    //{
                    //    if ((TYPE(data) != PYTHON_NIL) && (ref_data_type [curr->param_num - 1] != TYPE(data)))
                    //    {
                    //        sprintf(error, "Value parameters array %d is not homogeneous with privious parameters array", i + 1);
                    //        _build_client_err_list(head_error_list, error);
                    //        err_count++;
                    //        break;
                    //    }
                    //}
                    //else
                    {
                        if (TYPE(data) != PYTHON_NIL)
                        {
                            ref_data_type [curr->param_num - 1] = TYPE(data);
                        }
                        else
                        {
                            int i_tmp;
                            PyObject *param_tmp = NULL;
                            PyObject *data_tmp = NULL;
                            i_tmp = i + 1;
                            for (i_tmp = i + 1; i_tmp < numOfRows; i_tmp++)
                            {
                                param_tmp = PyTuple_GET_ITEM(params, i_tmp);
                                if (!PyTuple_Check(param_tmp))
                                {
                                    continue;
                                }
                                data_tmp = PyTuple_GET_ITEM(param_tmp, j);
                                if (TYPE(data_tmp) != PYTHON_NIL)
                                {
                                    ref_data_type [curr->param_num - 1] = TYPE(data_tmp);
                                    break;
                                }
                                else
                                {
                                    continue;
                                }
                            }
                            if (ref_data_type [curr->param_num - 1] == -1)
                            {
                                ref_data_type [curr->param_num - 1] = PYTHON_NIL;
                            }
                        }

                    }

                    curr->data_type = data_type [curr->param_num - 1];
                    if (TYPE(data) != PYTHON_NIL)
                    {
                        rc = _python_IfxPy_bind_data(stmt_res, curr, data);
                    }
                    else
                    {
                        SQLSMALLINT valueType = 0;
                        switch (ref_data_type [curr->param_num - 1])
                        {
                        case PYTHON_FIXNUM:
                            if (curr->data_type == SQL_BIGINT || curr->data_type == SQL_DECIMAL)
                            {
                                valueType = SQL_C_CHAR;
                            }
                            else
                            {
                                valueType = SQL_C_LONG;
                            }
                            break;
                        case PYTHON_FALSE:
                        case PYTHON_TRUE:
                            valueType = SQL_C_LONG;
                            break;
                        case PYTHON_FLOAT:
                            valueType = SQL_C_DOUBLE;
                            break;
                        case PYTHON_UNICODE:
                            switch (curr->data_type)
                            {
                            case SQL_BINARY:
                            case SQL_LONGVARBINARY:
                            case SQL_VARBINARY:
			    case SQL_INFX_RC_MULTISET:
			    case SQL_INFX_RC_SET:
			    case SQL_INFX_RC_ROW:
			    case SQL_INFX_RC_LIST:
			    case SQL_INFX_RC_COLLECTION:
		     	    case SQL_INFX_UDT_FIXED:
           		    case SQL_INFX_UDT_VARYING:	
                                valueType = SQL_C_BINARY; 
                                break;
                            default:
                                valueType = SQL_C_WCHAR;
                            }
                            break;
                        case PYTHON_STRING:
                            switch (curr->data_type)
                            {
                            case SQL_BINARY:
                            case SQL_LONGVARBINARY:
                            case SQL_VARBINARY:
                            case SQL_INFX_RC_MULTISET:
                            case SQL_INFX_RC_SET:
			    case SQL_INFX_RC_ROW:
			    case SQL_INFX_RC_LIST:
			    case SQL_INFX_RC_COLLECTION:
			    case SQL_INFX_UDT_FIXED:
           		    case SQL_INFX_UDT_VARYING:
                                valueType = SQL_C_BINARY;
                                break;
                            default:
                                valueType = SQL_C_CHAR;
                            }
                            break;
                        case PYTHON_DATE:
                            valueType = SQL_C_TYPE_DATE;
                            break;
                        case PYTHON_TIME:
                            valueType = SQL_C_TYPE_TIME;
                            break;
                        case PYTHON_TIMESTAMP:
                            valueType = SQL_C_TYPE_TIMESTAMP;
                            break;
                        case PYTHON_TIMEDELTA:
                            valueType = SQL_C_INTERVAL_DAY_TO_SECOND;
                            break;
                        case PYTHON_DECIMAL:
                            valueType = SQL_C_CHAR;
                            break;
                        case PYTHON_NIL:
                            valueType = SQL_C_DEFAULT;
                            break;
                        }
                        curr->ivalue = SQL_NULL_DATA;

                        Py_BEGIN_ALLOW_THREADS;
                        rc = SQLBindParameter(stmt_res->hstmt, curr->param_num, curr->param_type, valueType, curr->data_type, curr->param_size, curr->scale, &curr->ivalue, 0, (SQLLEN *)&(curr->ivalue));
                        Py_END_ALLOW_THREADS;
                        if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
                        {
                            _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
                        }
                    }
                    if (rc != SQL_SUCCESS)
                    {
                        sprintf(error, "Binding Error 1: %s",
                                IFX_G(__python_stmt_err_msg));
                        _build_client_err_list(head_error_list, error);
                        err_count++;
                        break;
                    }
                    curr = curr->next;
                    j++;
                }

                //if (!chaining_start && (error [0] == '\0'))
                //{
                //    // Set statement attribute SQL_ATTR_CHAINING_BEGIN
                //    rc = _IfxPy_chaining_flag(stmt_res, SQL_ATTR_CHAINING_BEGIN, NULL, 0);
                //    chaining_start = 1;
                //    if (rc != SQL_SUCCESS)
                //    {
                //        return NULL;
                //    }
                //}

                if (error [0] == '\0')
                {
                    Py_BEGIN_ALLOW_THREADS;
                    rc = SQLExecute((SQLHSTMT)stmt_res->hstmt);
                    Py_END_ALLOW_THREADS;

                    if (rc == SQL_NEED_DATA)
                    {
                        SQLPOINTER valuePtr;
                        rc = SQLParamData((SQLHSTMT)stmt_res->hstmt, (SQLPOINTER *)&valuePtr);
                        while (rc == SQL_NEED_DATA)
                        {
                            // passing data value for a parameter
                            if (!NIL_P(((param_node*)valuePtr)->svalue))
                            {
                                Py_BEGIN_ALLOW_THREADS;
                                rc = SQLPutData((SQLHSTMT)stmt_res->hstmt, (SQLPOINTER)(((param_node*)valuePtr)->svalue), ((param_node*)valuePtr)->ivalue);
                                Py_END_ALLOW_THREADS;
                            }
                            else
                            {
                                Py_BEGIN_ALLOW_THREADS;
                                rc = SQLPutData((SQLHSTMT)stmt_res->hstmt, (SQLPOINTER)(((param_node*)valuePtr)->uvalue), ((param_node*)valuePtr)->ivalue);
                                Py_END_ALLOW_THREADS;
                            }
                            if (rc == SQL_ERROR)
                            {
                                _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
                                sprintf(error, "Sending data failed: %s", IFX_G(__python_stmt_err_msg));
                                _build_client_err_list(head_error_list, error);
                                err_count++;
                                break;
                            }
                            rc = SQLParamData((SQLHSTMT)stmt_res->hstmt, (SQLPOINTER *)&valuePtr);
                        }
                    }
                }
            }
        }
        else
        {
            return PyInt_FromLong(0);

        }

        // Set statement attribute SQL_ATTR_CHAINING_END 
        //rc = _IfxPy_chaining_flag(stmt_res, SQL_ATTR_CHAINING_END, head_error_list->next, err_count);
        //if (head_error_list != NULL)
        //{
        //    error_msg_node *tmp_err = NULL;
        //    while (head_error_list != NULL)
        //    {
        //        tmp_err = head_error_list;
        //        head_error_list = head_error_list->next;
        //        PyMem_Del(tmp_err);
        //    }
        //}
        //if (rc != SQL_SUCCESS || err_count != 0)
        //{
        //    return NULL;
        //}
    }
    else
    {
        PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
        return NULL;

    }

    Py_BEGIN_ALLOW_THREADS;
    rc = SQLRowCount((SQLHSTMT)stmt_res->hstmt, &row_cnt);
    Py_END_ALLOW_THREADS;

    if ((rc == SQL_ERROR) && (stmt_res != NULL))
    {
        _python_IfxPy_check_sql_errors(stmt_res->hstmt, SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
        sprintf(error, "SQLRowCount failed: %s", IFX_G(__python_stmt_err_msg));
        PyErr_SetString(PyExc_Exception, error);
        return NULL;
    }
    return PyInt_FromLong(row_cnt);
}



/*
* ===Description
*  IfxPy.callproc( conn_handle conn_res, char *procName, (In/INOUT/OUT parameters tuple) )
*
* Returns resultset and INOUT/OUT parameters
*
* ===Parameters
* =====  conn_handle
*        a valid connection resource
* ===== procedure Name
*        a valide procedure Name
*
* ===== parameters tuple
*        parameters tuple containing In/OUT/INOUT  variables,
*
* ===Returns Values
* ===== stmt_res
*        statement resource containning result set
*
* ==== INOUT/OUT variables tuple
*        tuple containing all INOUT/OUT variables
*
* If procedure not found than it return NULL
*/
static PyObject* IfxPy_callproc(PyObject *self, PyObject *args)
{
    PyObject *py_conn_res = NULL;
    PyObject *parameters_tuple = NULL;
    PyObject *outTuple = NULL, *pyprocName = NULL, *data = NULL;
    conn_handle *conn_res = NULL;
    stmt_handle *stmt_res = NULL;
    param_node *tmp_curr = NULL;
    int numOfParam = 0;

    if (!PyArg_ParseTuple(args, "OO|O", &py_conn_res, &pyprocName, &parameters_tuple))
    {
        return NULL;
    }

    if (!NIL_P(py_conn_res) && pyprocName != Py_None)
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }
        if (StringObj_Size(pyprocName) == 0)
        {
            PyErr_SetString(PyExc_Exception, "Empty Procedure Name");
            return NULL;
        }

        if (!NIL_P(parameters_tuple))
        {
            PyObject *subsql1 = NULL;
            PyObject *subsql2 = NULL;
            char *strsubsql = NULL;
            PyObject *sql = NULL;
            int i = 0;
            if (!PyTuple_Check(parameters_tuple))
            {
                PyErr_SetString(PyExc_Exception, "Param is not a tuple");
                return NULL;
            }
            numOfParam = (int)PyTuple_Size(parameters_tuple);
            subsql1 = StringOBJ_FromASCII("CALL ");
           
            subsql2 = PyUnicode_Concat(subsql1, pyprocName);
            Py_XDECREF(subsql1);
            strsubsql = (char *)PyMem_Malloc(sizeof(char)*((strlen("(  )") + strlen(", ?")*numOfParam) + 2));
            if (strsubsql == NULL)
            {
                PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
                return NULL;
            }
            strsubsql [0] = '\0';
            strcat(strsubsql, "( ");
            for (i = 0; i < numOfParam; i++)
            {
                if (i == 0)
                {
                    strcat(strsubsql, " ?");
                }
                else
                {
                    strcat(strsubsql, ", ?");
                }
            }
            strcat(strsubsql, " ) ");
            subsql1 = StringOBJ_FromASCII(strsubsql);
            sql = PyUnicode_Concat(subsql2, subsql1);
            Py_XDECREF(subsql1);
            Py_XDECREF(subsql2);
            stmt_res = (stmt_handle *)_python_IfxPy_prepare_helper(conn_res, sql, NULL);
            PyMem_Del(strsubsql);
            Py_XDECREF(sql);
            if (NIL_P(stmt_res))
            {
                return NULL;
            }
            /* Bind values from the parameters_tuple to params */
            for (i = 0; i < numOfParam; i++)
            {
                PyObject *bind_result = NULL;
                data = PyTuple_GET_ITEM(parameters_tuple, i);
                bind_result = _python_IfxPy_bind_param_helper(4, stmt_res, i + 1, data, SQL_PARAM_INPUT_OUTPUT, 0, 0, 0, 0);
                if (NIL_P(bind_result))
                {
                    return NULL;
                }
            }
        }
        else
        {
            PyObject *subsql1 = NULL;
            PyObject *subsql2 = NULL;
            PyObject *sql = NULL;
            subsql1 = StringOBJ_FromASCII("CALL ");
            subsql2 = PyUnicode_Concat(subsql1, pyprocName);
            Py_XDECREF(subsql1);
            subsql1 = StringOBJ_FromASCII("( )");
            sql = PyUnicode_Concat(subsql2, subsql1);
            Py_XDECREF(subsql1);
            Py_XDECREF(subsql2);
            stmt_res = (stmt_handle *)_python_IfxPy_prepare_helper(conn_res, sql, NULL);
            Py_XDECREF(sql);
            if (NIL_P(stmt_res))
            {
                return NULL;
            }
        }

        if (!NIL_P(_python_IfxPy_execute_helper1(stmt_res, NULL)))
        {
            tmp_curr = stmt_res->head_cache_list;
            if (numOfParam != 0 && tmp_curr != NULL)
            {
                int paramCount = 1;
                outTuple = PyTuple_New(numOfParam + 1);
                PyTuple_SetItem(outTuple, 0, (PyObject*)stmt_res);
                while (tmp_curr != NULL && (paramCount <= numOfParam))
                {
                    if ((tmp_curr->bind_indicator != SQL_NULL_DATA && tmp_curr->bind_indicator != SQL_NO_TOTAL))
                    {
                        switch (tmp_curr->data_type)
                        {
                        case SQL_SMALLINT:
                        case SQL_INTEGER:
                            if (tmp_curr->ivalue != 0)
                            {
                                PyTuple_SetItem(outTuple, paramCount,
                                                PyInt_FromLong(tmp_curr->ivalue));
                            }
                            else
                            {
                                Py_INCREF(Py_None);
                                PyTuple_SetItem(outTuple, paramCount, Py_None);
                            }
                            paramCount++;
                            break;
                        case SQL_REAL:
                        case SQL_FLOAT:
                        case SQL_DOUBLE:
                            PyTuple_SetItem(outTuple, paramCount,
                                            PyFloat_FromDouble(tmp_curr->fvalue));
                            paramCount++;
                            break;
                        case SQL_TYPE_DATE:
                            if (!NIL_P(tmp_curr->date_value))
                            {
                                PyTuple_SetItem(outTuple, paramCount,
                                                PyDate_FromDate(tmp_curr->date_value->year,
                                                tmp_curr->date_value->month,
                                                tmp_curr->date_value->day));
                            }
                            else
                            {
                                Py_INCREF(Py_None);
                                PyTuple_SetItem(outTuple, paramCount, Py_None);
                            }
                            paramCount++;
                            break;
                        case SQL_TYPE_TIME:
                            if (!NIL_P(tmp_curr->time_value))
                            {
                                PyTuple_SetItem(outTuple, paramCount,
                                                PyTime_FromTime(tmp_curr->time_value->hour,
                                                tmp_curr->time_value->minute,
                                                tmp_curr->time_value->second, 0));
                            }
                            else
                            {
                                Py_INCREF(Py_None);
                                PyTuple_SetItem(outTuple, paramCount, Py_None);
                            }
                            paramCount++;
                            break;
                        case SQL_TYPE_TIMESTAMP:
                            if (!NIL_P(tmp_curr->ts_value))
                            {
                                PyTuple_SetItem(outTuple, paramCount,
                                                PyDateTime_FromDateAndTime(tmp_curr->ts_value->year,
                                                tmp_curr->ts_value->month, tmp_curr->ts_value->day,
                                                tmp_curr->ts_value->hour,
                                                tmp_curr->ts_value->minute,
                                                tmp_curr->ts_value->second,
                                                tmp_curr->ts_value->fraction / 1000));
                            }
                            else
                            {
                                Py_INCREF(Py_None);
                                PyTuple_SetItem(outTuple, paramCount, Py_None);
                            }
                            paramCount++;
                            break;
                        case SQL_BIGINT:
                            if (!NIL_P(tmp_curr->svalue))
                            {
                                PyTuple_SetItem(outTuple, paramCount,
                                                PyLong_FromString(tmp_curr->svalue,
                                                NULL, 0));
                            }
                            else
                            {
                                Py_INCREF(Py_None);
                                PyTuple_SetItem(outTuple, paramCount, Py_None);
                            }
                            paramCount++;
                            break;
                        default:
                            if (!NIL_P(tmp_curr->svalue))
                            {
                                PyTuple_SetItem(outTuple, paramCount, StringOBJ_FromASCII(tmp_curr->svalue));
                                paramCount++;
                            }
                            else if (!NIL_P(tmp_curr->uvalue))
                            {
                                PyTuple_SetItem(outTuple, paramCount, getSQLWCharAsPyUnicodeObject(tmp_curr->uvalue, tmp_curr->bind_indicator));
                                paramCount++;
                            }
                            else
                            {
                                Py_INCREF(Py_None);
                                PyTuple_SetItem(outTuple, paramCount, Py_None);
                                paramCount++;
                            }
                            break;
                        }
                    }
                    else
                    {
                        Py_INCREF(Py_None);
                        PyTuple_SetItem(outTuple, paramCount, Py_None);
                        paramCount++;
                    }
                    tmp_curr = tmp_curr->next;
                }
            }
            else
            {
                outTuple = (PyObject *)stmt_res;
            }
        }
        else
        {
            return NULL;
        }
        return outTuple;
    }
    else
    {
        PyErr_SetString(PyExc_Exception, "Connection Resource invalid or procedure name is NULL");
        return NULL;
    }
}


/*
* IfxPy.check_function_support-- can be used to query whether a  ODBC function is supported
* ===Description
* int IfxPy.check_function_support(ConnectionHandle, FunctionId)
* Returns Py_True if a ODBC function is supported
* return Py_False if a ODBC function is not supported
*/
static PyObject* IfxPy_check_function_support(PyObject *self, PyObject *args)
{
    PyObject *py_conn_res = NULL;
    PyObject *py_funtion_id = NULL;
    int funtion_id = 0;
    conn_handle *conn_res = NULL;
    int supported = 0;
    int rc = 0;

    if (!PyArg_ParseTuple(args, "OO", &py_conn_res, &py_funtion_id))
    {
        return NULL;
    }

    if (!NIL_P(py_conn_res))
    {
        if (!PyObject_TypeCheck(py_conn_res, &conn_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied connection object Parameter is invalid");
            return NULL;
        }
        else
        {
            conn_res = (conn_handle *)py_conn_res;
        }
        if (!NIL_P(py_funtion_id))
        {
            if (PyInt_Check(py_funtion_id))
            {
                funtion_id = (int)PyInt_AsLong(py_funtion_id);
            }
            else
            {
                PyErr_SetString(PyExc_Exception, "Supplied parameter is invalid");
                return NULL;
            }
        }
        /* Check to ensure the connection resource given is active */
        if (!conn_res->handle_active)
        {
            PyErr_SetString(PyExc_Exception, "Connection is not active");
            return NULL;
        }

        Py_BEGIN_ALLOW_THREADS;
        rc = SQLGetFunctions(conn_res->hdbc, (SQLUSMALLINT)funtion_id, (SQLUSMALLINT*)&supported);
        Py_END_ALLOW_THREADS;

        if (rc == SQL_ERROR)
        {
            Py_RETURN_FALSE;
        }
        else
        {
            if (supported == SQL_TRUE)
            {
                Py_RETURN_TRUE;
            }
            else
            {
                Py_RETURN_FALSE;
            }
        }

    }
    return NULL;
}

/*
* IfxPy.get_last_serial_value --    Gets the last inserted serial value from IDS
*
* ===Description
* string IfxPy.get_last_serial_value ( resource stmt )
*
* Returns a string, that is the last inserted value for a serial column for IDS.
* The last inserted value could be auto-generated or entered explicitly by the user
* This function is valid for IDS (Informix Dynamic Server only)
*
* ===Parameters
*
* stmt
*        A valid statement resource.
*
* ===Return Values
*
* Returns a string representation of last inserted serial value on a successful call.
* Returns FALSE on failure.
*/
PyObject *IfxPy_get_last_serial_value(int argc, PyObject *args, PyObject *self)
{
    PyObject *stmt = NULL;
    SQLCHAR *value = NULL;
    PyObject *return_value = NULL;
    SQLINTEGER pcbValue = 0;
    stmt_handle *stmt_res;
    int rc = 0;


    PyObject *py_qualifier = NULL;
    PyObject *retVal = NULL;

    if (!PyArg_ParseTuple(args, "O", &py_qualifier))
        return NULL;

    if (!NIL_P(py_qualifier))
    {
        if (!PyObject_TypeCheck(py_qualifier, &stmt_handleType))
        {
            PyErr_SetString(PyExc_Exception, "Supplied statement object parameter is invalid");
            return NULL;
        }
        else
        {
            stmt_res = (stmt_handle *)py_qualifier;
        }

        /* We allocate a buffer of size 31 as per recommendations from the CLI IDS team */
        value = ALLOC_N(char, 31);
        if (value == NULL)
        {
            PyErr_SetString(PyExc_Exception, "Failed to Allocate Memory");
            return Py_False;
        }
        rc = SQLGetStmtAttr((SQLHSTMT)stmt_res->hstmt, SQL_ATTR_GET_GENERATED_VALUE, (SQLPOINTER)value, 31, &pcbValue);
        if (rc == SQL_ERROR)
        {
            _python_IfxPy_check_sql_errors((SQLHSTMT)stmt_res->hstmt, SQL_HANDLE_STMT, rc, 1, NULL, -1, 1);
            if (value != NULL)
            {
                PyMem_Del(value);
                value = NULL;
            }
            PyErr_Clear();
            return Py_False;
        }
        retVal = StringOBJ_FromASCII((char *)value);
        if (value != NULL)
        {
            PyMem_Del(value);
            value = NULL;
        }
        return retVal;
    }
    else
    {
        PyErr_SetString(PyExc_Exception, "Supplied statement handle is invalid");
        return Py_False;
    }
}

static int _python_get_variable_type(PyObject *variable_value)
{
    if (PyBool_Check(variable_value) && (variable_value == Py_True))
    {
        return PYTHON_TRUE;
    }
    else if (PyBool_Check(variable_value) && (variable_value == Py_False))
    {
        return PYTHON_FALSE;
    }
    else if (PyInt_Check(variable_value) || PyLong_Check(variable_value))
    {
        return PYTHON_FIXNUM;
    }
    else if (PyFloat_Check(variable_value))
    {
        return PYTHON_FLOAT;
    }
    else if (PyUnicode_Check(variable_value))
    {
        return PYTHON_UNICODE;
    }
    else if (PyString_Check(variable_value) || PyBytes_Check(variable_value))
    {
        return PYTHON_STRING;
    }
    else if (PyDateTime_Check(variable_value))
    {
        return PYTHON_TIMESTAMP;
    }
    else if (PyDelta_Check(variable_value))
    {
        return PYTHON_TIMEDELTA;
    }
    else if (PyTime_Check(variable_value))
    {
        return PYTHON_TIME;
    }
    else if (PyDate_Check(variable_value))
    {
        return PYTHON_DATE;
    }
    else if (PyComplex_Check(variable_value))
    {
        return PYTHON_COMPLEX;
    }
    else if (PyNumber_Check(variable_value))
    {
        return PYTHON_DECIMAL;
    }
    else if (variable_value == Py_None)
    {
        return PYTHON_NIL;
    }
    else return 0;
}

//////////////////////////////////////////////////////////////
// We can use this for SPEED TEST between Py and C
// Find the number of prime numbers between X and Y
static PyObject *SpeedTestWithCPrimeCount(PyObject *self, PyObject *args)
{
	int i = 0;
	int j = 0;
	int x = 0;
	int y = 0;
	int VRange = 0;
	int isPrime = 0;
	int PrimeCount = 0;

	//printf("\n");
	if (!PyArg_ParseTuple(args, "ii", &x, &y))
	{
		return NULL;
	}

	if (x < 2)
		x = 2;

	++y;
	for ( i=x; i<y; i++)
	{
		isPrime = 1;

		VRange = i / 2; //This Validation Range is good enough
		++VRange;
		for (j=2; j<VRange; j++)
		{
			// Check whether it is  divisible by any number other than 1
			if ( !(i%j) )
			{
				isPrime = 0;
				break;
			}
		}

		if (isPrime)
		{
			//printf(" [%d] ", i);
			++PrimeCount;
		}
	}

	PyObject *ret = Py_BuildValue("i", PrimeCount);
	return ret;
}


//////////////////////////////////////////////////////////////


/* Listing of IfxPy module functions: */
static PyMethodDef IfxPy_Methods[] = {
    /* name, function, argument type, docstring */
    { "connect", (PyCFunction)IfxPy_connect, METH_VARARGS | METH_KEYWORDS, "Connect to the database" },
    { "exec_immediate", (PyCFunction)IfxPy_exec, METH_VARARGS, "Prepares and executes an SQL statement." },
    { "prepare", (PyCFunction)IfxPy_prepare, METH_VARARGS, "Prepares an SQL statement." },
    { "bind_param", (PyCFunction)IfxPy_bind_param, METH_VARARGS, "Binds a Python variable to an SQL statement parameter" },
    { "execute", (PyCFunction)IfxPy_execute, METH_VARARGS, "Executes an SQL statement that was prepared by IfxPy.prepare()" },
    { "fetch_tuple", (PyCFunction)IfxPy_fetch_array, METH_VARARGS, "Returns an tuple, indexed by column position, representing a row in a result set" },
    { "fetch_assoc", (PyCFunction)IfxPy_fetch_assoc, METH_VARARGS, "Returns a dictionary, indexed by column name, representing a row in a result set" },
    { "fetch_both", (PyCFunction)IfxPy_fetch_both, METH_VARARGS, "Returns a dictionary, indexed by both column name and position, representing a row in a result set" },
    { "fetch_row", (PyCFunction)IfxPy_fetch_row, METH_VARARGS, "Sets the result set pointer to the next row or requested row" },
    { "result", (PyCFunction)IfxPy_result, METH_VARARGS, "Returns a single column from a row in the result set" },
    { "active", (PyCFunction)IfxPy_active, METH_VARARGS, "Checks if the specified connection resource is active" },
    { "autocommit", (PyCFunction)IfxPy_autocommit, METH_VARARGS, "Returns or sets the AUTOCOMMIT state for a database connection" },
    { "callproc", (PyCFunction)IfxPy_callproc, METH_VARARGS, "Returns a tuple containing OUT/INOUT variable value" },
    { "check_function_support", (PyCFunction)IfxPy_check_function_support, METH_VARARGS, "return true if fuction is supported otherwise return false" },
    { "close", (PyCFunction)IfxPy_close, METH_VARARGS, "Close a database connection" },
    { "conn_error", (PyCFunction)IfxPy_conn_error, METH_VARARGS, "Returns a string containing the SQLSTATE returned by the last connection attempt" },
    { "conn_errormsg", (PyCFunction)IfxPy_conn_errormsg, METH_VARARGS, "Returns an error message and SQLCODE value representing the reason the last database connection attempt failed" },
    { "conn_warn", (PyCFunction)IfxPy_conn_warn, METH_VARARGS, "Returns a warning string containing the SQLSTATE returned by the last connection attempt" },
    { "client_info", (PyCFunction)IfxPy_client_info, METH_VARARGS, "Returns a read-only object with information about the IDS database client" },
    { "column_privileges", (PyCFunction)IfxPy_column_privileges, METH_VARARGS, "Returns a result set listing the columns and associated privileges for a table." },
    { "columns", (PyCFunction)IfxPy_columns, METH_VARARGS, "Returns a result set listing the columns and associated metadata for a table" },
    { "commit", (PyCFunction)IfxPy_commit, METH_VARARGS, "Commits a transaction" },
    { "cursor_type", (PyCFunction)IfxPy_cursor_type, METH_VARARGS, "Returns the cursor type used by a statement resource" },
    { "execute_many", (PyCFunction)IfxPy_execute_many, METH_VARARGS, "Execute SQL with multiple rows." }, // TODO
    { "field_display_size", (PyCFunction)IfxPy_field_display_size, METH_VARARGS, "Returns the maximum number of bytes required to display a column" },
    { "field_name", (PyCFunction)IfxPy_field_name, METH_VARARGS, "Returns the name of the column in the result set" },
    { "field_nullable", (PyCFunction)IfxPy_field_nullable, METH_VARARGS, "Returns indicated column can contain nulls or not" },
    { "field_num", (PyCFunction)IfxPy_field_num, METH_VARARGS, "Returns the position of the named column in a result set" },
    { "field_precision", (PyCFunction)IfxPy_field_precision, METH_VARARGS, "Returns the precision of the indicated column in a result set" },
    { "field_scale", (PyCFunction)IfxPy_field_scale, METH_VARARGS, "Returns the scale of the indicated column in a result set" },
    { "field_type", (PyCFunction)IfxPy_field_type, METH_VARARGS, "Returns the data type of the indicated column in a result set" },
    { "field_width", (PyCFunction)IfxPy_field_width, METH_VARARGS, "Returns the width of the indicated column in a result set" },
    { "foreign_keys", (PyCFunction)IfxPy_foreign_keys, METH_VARARGS, "Returns a result set listing the foreign keys for a table" },
    { "free_result", (PyCFunction)IfxPy_free_result, METH_VARARGS, "Frees resources associated with a result set" },
    { "free_stmt", (PyCFunction)IfxPy_free_stmt, METH_VARARGS, "Frees resources associated with the indicated statement resource" },
    { "get_option", (PyCFunction)IfxPy_get_option, METH_VARARGS, "Gets the specified option in the resource." },
    { "num_fields", (PyCFunction)IfxPy_num_fields, METH_VARARGS, "Returns the number of fields contained in a result set" },
    { "num_rows", (PyCFunction)IfxPy_num_rows, METH_VARARGS, "Returns the number of rows affected by an SQL statement" },
    { "get_num_result", (PyCFunction)IfxPy_get_num_result, METH_VARARGS, "Returns the number of rows in a current open non-dynamic scrollable cursor" },
    { "primary_keys", (PyCFunction)IfxPy_primary_keys, METH_VARARGS, "Returns a result set listing primary keys for a table" },
    { "procedure_columns", (PyCFunction)IfxPy_procedure_columns, METH_VARARGS, "Returns a result set listing the parameters for one or more stored procedures." },
    { "procedures", (PyCFunction)IfxPy_procedures, METH_VARARGS, "Returns a result set listing the stored procedures registered in a database" },
    { "rollback", (PyCFunction)IfxPy_rollback, METH_VARARGS, "Rolls back a transaction" },
    { "server_info", (PyCFunction)IfxPy_server_info, METH_VARARGS, "Returns an object with properties that describe the IDS database server" },
    { "get_db_info", (PyCFunction)IfxPy_get_db_info, METH_VARARGS, "Returns an object with properties that describe the IDS database server according to the option passed" },
    { "set_option", (PyCFunction)IfxPy_set_option, METH_VARARGS, "Sets the specified option in the resource" },
    { "special_columns", (PyCFunction)IfxPy_special_columns, METH_VARARGS, "Returns a result set listing the unique row identifier columns for a table" },
    { "statistics", (PyCFunction)IfxPy_statistics, METH_VARARGS, "Returns a result set listing the index and statistics for a table" },
    { "stmt_error", (PyCFunction)IfxPy_stmt_error, METH_VARARGS, "Returns a string containing the SQLSTATE returned by an SQL statement" },
    { "stmt_warn", (PyCFunction)IfxPy_stmt_warn, METH_VARARGS, "Returns a warning string containing the SQLSTATE returned by last SQL statement" },
    { "stmt_errormsg", (PyCFunction)IfxPy_stmt_errormsg, METH_VARARGS, "Returns a string containing the last SQL statement error message" },
    { "table_privileges", (PyCFunction)IfxPy_table_privileges, METH_VARARGS, "Returns a result set listing the tables and associated privileges in a database" },
    { "tables", (PyCFunction)IfxPy_tables, METH_VARARGS, "Returns a result set listing the tables and associated metadata in a database" },
    { "get_last_serial_value", (PyCFunction)IfxPy_get_last_serial_value, METH_VARARGS, "Returns last serial value inserted for identity column" },
    { "SpeedTestWithCPrimeCount", (PyCFunction)SpeedTestWithCPrimeCount, METH_VARARGS, "Used for Speed Test C & Python function by counting number of prime numbers between X and Y" },
#ifdef HAVE_SMARTTRIGGER
    { "open_smart_trigger", (PyCFunction)IfxPy_open_smart_trigger, METH_VARARGS, "Open Smart Trigger session" },
	{ "get_smart_trigger_session_id", (PyCFunction)IfxPy_get_smart_trigger_sessionID, METH_VARARGS, "Get already opened Smart Trigger session ID" },
	{ "join_smart_trigger_session", (PyCFunction)IfxPy_join_smart_trigger_session, METH_VARARGS, "Join already opened Smart Trigger session ID" },
	{ "register_smart_trigger_loop", (PyCFunction)IfxPy_register_smart_trigger_loop, METH_VARARGS, "Open Smart Trigger session with loop handled by Python" },
	{ "register_smart_trigger_no_loop", (PyCFunction)IfxPy_register_smart_trigger_no_loop, METH_VARARGS, "Open Smart Trigger session with loop handled by application" },
	{ "delete_smart_trigger_session", (PyCFunction)IfxPy_delete_smart_trigger_session, METH_VARARGS, "Delete opened Smart Trigger session" },
#endif  // HAVE_SMARTTRIGGER
    // An end-of-listing sentinel:
    { NULL, NULL, 0, NULL }
};

#ifndef PyMODINIT_FUNC    /* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif

static const char DriverModuleName[] = "IfxPy";
static const char DriverModuleDescription[] = "Informix Native Driver for Python.";

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    DriverModuleName,
    DriverModuleDescription,
    -1,
    IfxPy_Methods,
};
#endif

/* Module initialization function */
PyMODINIT_FUNC
INIT_IfxPy(void)
{
    PyObject* m;

    PyDateTime_IMPORT;
    IfxPy_globals = ALLOC(struct _IfxPy_globals);
    memset(IfxPy_globals, 0, sizeof(struct _IfxPy_globals));
    python_IfxPy_init_globals(IfxPy_globals);

    persistent_list = PyDict_New();

    conn_handleType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&conn_handleType) < 0)
        return MOD_RETURN_ERROR;

    stmt_handleType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&stmt_handleType) < 0)
        return MOD_RETURN_ERROR;

    client_infoType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&client_infoType) < 0)
        return MOD_RETURN_ERROR;

    server_infoType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&server_infoType) < 0)
        return MOD_RETURN_ERROR;

#if PY_MAJOR_VERSION < 3
    //m = Py_InitModule3("IfxPy", IfxPy_Methods, "Informix Native Driver for Python.");
    m = Py_InitModule3(DriverModuleName, IfxPy_Methods, DriverModuleDescription);

#else
    m = PyModule_Create(&moduledef);
#endif

    Py_INCREF(&conn_handleType);
    PyModule_AddObject(m, "IFXConnection", (PyObject *)&conn_handleType);

    PyModule_AddIntConstant(m, "SQL_AUTOCOMMIT_ON", SQL_AUTOCOMMIT_ON);
    PyModule_AddIntConstant(m, "SQL_AUTOCOMMIT_OFF", SQL_AUTOCOMMIT_OFF);
    PyModule_AddIntConstant(m, "SQL_ATTR_AUTOCOMMIT", SQL_ATTR_AUTOCOMMIT);
    PyModule_AddIntConstant(m, "ATTR_CASE", ATTR_CASE);
    PyModule_AddIntConstant(m, "CASE_NATURAL", CASE_NATURAL);
    PyModule_AddIntConstant(m, "CASE_LOWER", CASE_LOWER);
    PyModule_AddIntConstant(m, "CASE_UPPER", CASE_UPPER);
    PyModule_AddIntConstant(m, "USE_WCHAR", USE_WCHAR);
    PyModule_AddIntConstant(m, "WCHAR_YES", WCHAR_YES);
    PyModule_AddIntConstant(m, "WCHAR_NO", WCHAR_NO);
    PyModule_AddIntConstant(m, "SQL_ATTR_CURSOR_TYPE", SQL_ATTR_CURSOR_TYPE);
    PyModule_AddIntConstant(m, "SQL_CURSOR_FORWARD_ONLY", SQL_CURSOR_FORWARD_ONLY);
    PyModule_AddIntConstant(m, "SQL_CURSOR_KEYSET_DRIVEN", SQL_CURSOR_KEYSET_DRIVEN);
    PyModule_AddIntConstant(m, "SQL_CURSOR_DYNAMIC", SQL_CURSOR_DYNAMIC);
    PyModule_AddIntConstant(m, "SQL_CURSOR_STATIC", SQL_CURSOR_STATIC);
    PyModule_AddIntConstant(m, "SQL_PARAM_INPUT", SQL_PARAM_INPUT);
    PyModule_AddIntConstant(m, "SQL_PARAM_OUTPUT", SQL_PARAM_OUTPUT);
    PyModule_AddIntConstant(m, "SQL_PARAM_INPUT_OUTPUT", SQL_PARAM_INPUT_OUTPUT);
    PyModule_AddIntConstant(m, "PARAM_FILE", PARAM_FILE);

    PyModule_AddIntConstant(m, "SQL_BIGINT", SQL_BIGINT);
    PyModule_AddIntConstant(m, "SQL_BINARY", SQL_BINARY);
    PyModule_AddIntConstant(m, "SQL_CHAR", SQL_CHAR);
    PyModule_AddIntConstant(m, "SQL_TINYINT", SQL_TINYINT);
    PyModule_AddIntConstant(m, "SQL_BINARY", SQL_BINARY);
    PyModule_AddIntConstant(m, "SQL_BIT", SQL_BIT);
    PyModule_AddIntConstant(m, "SQL_TYPE_DATE", SQL_TYPE_DATE);
    PyModule_AddIntConstant(m, "SQL_DECIMAL", SQL_DECIMAL);
    PyModule_AddIntConstant(m, "SQL_DOUBLE", SQL_DOUBLE);
    PyModule_AddIntConstant(m, "SQL_FLOAT", SQL_FLOAT);
    PyModule_AddIntConstant(m, "SQL_INTEGER", SQL_INTEGER);
    PyModule_AddIntConstant(m, "SQL_LONGVARCHAR", SQL_LONGVARCHAR);
    PyModule_AddIntConstant(m, "SQL_LONGVARBINARY", SQL_LONGVARBINARY);
    PyModule_AddIntConstant(m, "SQL_WLONGVARCHAR", SQL_WLONGVARCHAR);
    PyModule_AddIntConstant(m, "SQL_NUMERIC", SQL_NUMERIC);
    PyModule_AddIntConstant(m, "SQL_REAL", SQL_REAL);
    PyModule_AddIntConstant(m, "SQL_SMALLINT", SQL_SMALLINT);
    PyModule_AddIntConstant(m, "SQL_TYPE_TIME", SQL_TYPE_TIME);
    PyModule_AddIntConstant(m, "SQL_TYPE_TIMESTAMP", SQL_TYPE_TIMESTAMP);
    PyModule_AddIntConstant(m, "SQL_VARBINARY", SQL_VARBINARY);
    PyModule_AddIntConstant(m, "SQL_VARCHAR", SQL_VARCHAR);
    PyModule_AddIntConstant(m, "SQL_VARBINARY", SQL_VARBINARY);
    PyModule_AddIntConstant(m, "SQL_WVARCHAR", SQL_WVARCHAR);
    PyModule_AddIntConstant(m, "SQL_WCHAR", SQL_WCHAR);
    PyModule_AddIntConstant(m, "SQL_FALSE", SQL_FALSE);
    PyModule_AddIntConstant(m, "SQL_TRUE", SQL_TRUE);
    PyModule_AddIntConstant(m, "SQL_TABLE_STAT", SQL_TABLE_STAT);
    PyModule_AddIntConstant(m, "SQL_INDEX_CLUSTERED", SQL_INDEX_CLUSTERED);
    PyModule_AddIntConstant(m, "SQL_INDEX_OTHER", SQL_INDEX_OTHER);
    PyModule_AddIntConstant(m, "SQL_DBMS_NAME", SQL_DBMS_NAME);
    PyModule_AddIntConstant(m, "SQL_DBMS_VER", SQL_DBMS_VER);
    PyModule_AddIntConstant(m, "SQL_API_SQLROWCOUNT", SQL_API_SQLROWCOUNT);
    PyModule_AddIntConstant(m, "SQL_INFX_RC_COLLECTION", SQL_INFX_RC_COLLECTION);	
    PyModule_AddIntConstant(m, "SQL_INFX_RC_LIST", SQL_INFX_RC_LIST);
    PyModule_AddIntConstant(m, "SQL_INFX_RC_SET", SQL_INFX_RC_SET);
    PyModule_AddIntConstant(m, "SQL_INFX_RC_MULTISET", SQL_INFX_RC_MULTISET);
    PyModule_AddIntConstant(m, "SQL_INFX_RC_ROW", SQL_INFX_RC_ROW);
    PyModule_AddIntConstant(m, "SQL_INFX_UDT_FIXED", SQL_INFX_UDT_FIXED);
    PyModule_AddIntConstant(m, "SQL_INFX_UDT_VARYING", SQL_INFX_UDT_VARYING);

    PyModule_AddStringConstant(m, "__version__", MODULE_RELEASE);

    Py_INCREF(&stmt_handleType);
    PyModule_AddObject(m, "IFXStatement", (PyObject *)&stmt_handleType);

    Py_INCREF(&client_infoType);
    PyModule_AddObject(m, "IFXClientInfo", (PyObject *)&client_infoType);

    Py_INCREF(&server_infoType);
    PyModule_AddObject(m, "IFXServerInfo", (PyObject *)&server_infoType);
    PyModule_AddIntConstant(m, "SQL_ATTR_QUERY_TIMEOUT", SQL_ATTR_QUERY_TIMEOUT);
    return MOD_RETURN_VAL(m);
}
