import sys, string
import dcerpc


def ResizeBufferCall(fn, pipe, r):

    r['buffer'] = None
    r['buf_size'] = 0
    
    result = fn(pipe, r)

    if result['result'] == dcerpc.WERR_INSUFFICIENT_BUFFER:
        r['buffer'] = result['buf_size'] * '\x00'
        r['buf_size'] = result['buf_size']

    result = fn(pipe, r)

    return result


def test_OpenPrinterEx(pipe, printer):

    print 'testing spoolss_OpenPrinterEx(%s)' % printer

    printername = '\\\\%s' % dcerpc.dcerpc_server_name(pipe)
    
    if printer is not None:
        printername = printername + '\\%s' % printer

    r = {}
    r['printername'] = printername
    r['datatype'] = None
    r['devmode_ctr'] = {}
    r['devmode_ctr']['size'] = 0
    r['devmode_ctr']['devmode'] = None
    r['access_mask'] = 0x02000000
    r['level'] = 1
    r['userlevel'] = {}
    r['userlevel']['level1'] = {}
    r['userlevel']['level1']['size'] = 0
    r['userlevel']['level1']['client'] = None
    r['userlevel']['level1']['user'] = None
    r['userlevel']['level1']['build'] = 1381
    r['userlevel']['level1']['major'] = 2
    r['userlevel']['level1']['minor'] = 0
    r['userlevel']['level1']['processor'] = 0

    result = dcerpc.spoolss_OpenPrinterEx(pipe, r)

    return result['handle']


def test_ClosePrinter(pipe, handle):

    r = {}
    r['handle'] = handle

    dcerpc.spoolss_ClosePrinter(pipe, r)


def test_GetPrinter(pipe, handle):

    r = {}
    r['handle'] = handle

    for level in [0, 1, 2, 3, 4, 5, 6, 7]:

        print 'test_GetPrinter(level = %d)' % level

        r['level'] = level
        r['buffer'] = None
        r['buf_size'] = 0

        result = ResizeBufferCall(dcerpc.spoolss_GetPrinter, pipe, r)


def test_EnumForms(pipe, handle):

    print 'testing spoolss_EnumForms'

    r = {}
    r['handle'] = handle
    r['level'] = 1
    r['buffer'] = None
    r['buf_size'] = 0

    result = ResizeBufferCall(dcerpc.spoolss_EnumForms, pipe, r)

    forms = dcerpc.unmarshall_spoolss_FormInfo_array(
        result['buffer'], r['level'], result['count'])

    for form in forms:

        r = {}
        r['handle'] = handle
        r['formname'] = form['info1']['formname']
        r['level'] = 1

        result = ResizeBufferCall(dcerpc.spoolss_GetForm, pipe, r)


def test_EnumPorts(pipe, handle):

    print 'testing spoolss_EnumPorts'

    r = {}
    r['handle'] = handle
    r['level'] = 1
    r['buffer'] = None
    r['buf_size'] = 0

    result = ResizeBufferCall(dcerpc.spoolss_EnumPorts, pipe, r)


def test_DeleteForm(pipe, handle, formname):

    r = {}
    r['handle'] = handle
    r['formname'] = formname

    dcerpc.spoolss_DeleteForm(pipe, r)


def test_GetForm(pipe, handle, formname):

    r = {}
    r['handle'] = handle
    r['formname'] = formname
    r['level'] = 1

    result = ResizeBufferCall(dcerpc.spoolss_GetForm, pipe, r)

    return result['info']['info1']
    

def test_SetForm(pipe, handle, form):

    print 'testing spoolss_SetForm'

    r = {}
    r['handle'] = handle
    r['level'] = 1
    r['formname'] = form['info1']['formname']
    r['info'] = form

    dcerpc.spoolss_SetForm(pipe, r)

    newform = test_GetForm(pipe, handle, r['formname'])

    if form['info1'] != newform:
        print 'SetForm: mismatch: %s != %s' % \
              (r['info']['info1'], f)
        sys.exit(1)


def test_AddForm(pipe, handle):

    print 'testing spoolss_AddForm'

    formname = '__testform__'

    r = {}
    r['handle'] = handle
    r['level'] = 1
    r['info'] = {}
    r['info']['info1'] = {}
    r['info']['info1']['formname'] = formname
    r['info']['info1']['flags'] = 0x0002
    r['info']['info1']['width'] = 100
    r['info']['info1']['length'] = 100
    r['info']['info1']['left'] = 0
    r['info']['info1']['top'] = 1000
    r['info']['info1']['right'] = 2000
    r['info']['info1']['bottom'] = 3000

    try:
        result = dcerpc.spoolss_AddForm(pipe, r)
    except dcerpc.WERROR, arg:
        if arg[0] == dcerpc.WERR_ALREADY_EXISTS:
            test_DeleteForm(pipe, handle, formname)
        result = dcerpc.spoolss_AddForm(pipe, r)

    f = test_GetForm(pipe, handle, formname)

    if r['info']['info1'] != f:
        print 'AddForm: mismatch: %s != %s' % \
              (r['info']['info1'], f)
        sys.exit(1)

    r['formname'] = formname

    test_SetForm(pipe, handle, r['info'])

    test_DeleteForm(pipe, handle, formname)


def test_EnumJobs(pipe, handle):

    print 'testing spoolss_EnumJobs'

    r = {}
    r['handle'] = handle
    r['firstjob'] = 0
    r['numjobs'] = 0xffffffff
    r['level'] = 1

    result = ResizeBufferCall(dcerpc.spoolss_EnumJobs, pipe, r)

    if result['buffer'] is None:
        return
    
    jobs = dcerpc.unmarshall_spoolss_JobInfo_array(
        result['buffer'], r['level'], result['count'])

    for job in jobs:

        s = {}
        s['handle'] = handle
        s['job_id'] = job['info1']['job_id']
        s['level'] = 1

        result = ResizeBufferCall(dcerpc.spoolss_GetJob, pipe, s)

        if result['info'] != job:
            print 'EnumJobs: mismatch: %s != %s' % (result['info'], job)
            sys.exit(1)
    

def test_EnumPrinters(pipe):

    print 'testing spoolss_EnumPrinters'

    printer_names = None

    r = {}
    r['flags'] = 0x02
    r['server'] = None

    for level in [0, 1, 2, 4, 5]:

        print 'test_EnumPrinters(level = %d)' % level

        r['level'] = level

        result = ResizeBufferCall(dcerpc.spoolss_EnumPrinters, pipe, r)

        printers = dcerpc.unmarshall_spoolss_PrinterInfo_array(
            result['buffer'], r['level'], result['count'])

        if level == 2:
            for p in printers:

                # A nice check is for the specversion in the
                # devicemode.  This has always been observed to be
                # 1025.

                if p['info2']['devmode']['specversion'] != 1025:
                    print 'test_EnumPrinters: specversion != 1025'
                    sys.exit(1)

    r['level'] = 1
    result = ResizeBufferCall(dcerpc.spoolss_EnumPrinters, pipe, r)
    
    for printer in dcerpc.unmarshall_spoolss_PrinterInfo_array(
        result['buffer'], r['level'], result['count']):

        if string.find(printer['info1']['name'], '\\\\') == 0:
            print 'Skipping remote printer %s' % printer['info1']['name']
            continue

        printername = string.split(printer['info1']['name'], ',')[0]

        handle = test_OpenPrinterEx(pipe, printername)

        test_GetPrinter(pipe, handle)
        test_EnumForms(pipe, handle)
        test_AddForm(pipe, handle)
        test_EnumJobs(pipe, handle)
        test_ClosePrinter(pipe, handle)


def test_PrintServer(pipe):
    
    handle = test_OpenPrinterEx(pipe, None)

    test_ClosePrinter(pipe, handle)
    

def runtests(binding, domain, username, password):
    
    print 'Testing SPOOLSS pipe'

    pipe = dcerpc.pipe_connect(binding,
            dcerpc.DCERPC_SPOOLSS_UUID, dcerpc.DCERPC_SPOOLSS_VERSION,
            domain, username, password)

    test_EnumPrinters(pipe)
    test_PrintServer(pipe)
