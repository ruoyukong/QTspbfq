import { useEffect, useState } from 'react';
import './App.css';
import {
  TextField,
  Button,
  Snackbar,
  Alert,
  TableContainer,
  Table,
  TableHead,
  TableRow,
  TableCell,
  TableBody,
  Paper,
  TablePagination,
  TableFooter,
} from '@mui/material';
import { useConfirm } from 'material-ui-confirm';

const baseURL = 'https://prod.unicorn.org.cn/cephalon/user-center';

function App() {
  const confirm = useConfirm();

  const [initLoading, setInitLoading] = useState(true);
  const [phone, setPhone] = useState('');
  const [password, setPassword] = useState('');
  const [isLogged, setIsLogged] = useState(false);

  // 已启动列表
  const [runningApps, setRunningApps] = useState([]);
  const [pagination, setPagination] = useState({ pageIndex: 1, pageSize: 10, total: 0 });
  const columns = [
    { key: 'id', label: 'ID' },
    { key: 'category', label: '应用大类' },
    { key: 'gpuVersion', label: 'GPU 性能' },
    { key: 'url', label: '地址' },
    { key: 'status', label: '状态' },
    { key: 'startedAt', label: '启动时间' },
    { key: 'updatedAt', label: '更新时间' },
    { key: 'action', label: '操作' },
  ];

  // toast
  const [toastState, setToastState] = useState({ open: false, severity: 'success', message: '' });

  useEffect(() => {
    setInitLoading(true);
    const token = localStorage.getItem('token');
    if (token) {
      setIsLogged(true);
    }
    setInitLoading(false);
  }, []);

  useEffect(() => {
    if (isLogged) {
      fetchRunningApps();
    } else {
      setRunningApps([]);
    }
  }, [isLogged, pagination.pageIndex, pagination.pageSize]);

  async function login() {
    const body = { phone, pwd: password, way: 'phone_pwd' };
    const result = await fetch(`${baseURL}/v1/login`, {
      method: 'POST',
      body: JSON.stringify(body),
      headers: { 'Content-Type': 'application/json' },
    });
    if (result.ok) {
      const { code, data, msg } = await result.json();
      console.log(data, 'data');
      if (code === 20000) {
        setIsLogged(true);
        localStorage.setItem('token', data.token);
        setToastState({ ...toastState, open: true, severity: 'success', message: '登录成功' });
      } else {
        setToastState({ ...toastState, open: true, severity: 'error', message: msg });
      }
    }
  }

  async function findGPU() {
    const result = await fetch(`${baseURL}/v1/prices/cheapest?mission_billing_type=time&mission_category=SSH`);

    if (result.ok) {
      const { code, data } = await result.json();
      if (code === 20000) {
        const temp = (data || []).filter((item) => {
          const { is_deprecated, is_deprecated_super_node } = item;
          return !is_deprecated || !is_deprecated_super_node;
        });
        const quickStart = temp.filter((item) => item.quick_start);
        if (quickStart.length) {
          const i = Math.floor(Math.random() * quickStart.length);
          return quickStart[i];
        }
        const i = Math.floor(Math.random() * temp.length);
        return temp[i];
      }
    }
  }

  async function createSSHApp() {
    const token = localStorage.getItem('token');

    const gpu = await findGPU();
    if (!gpu) {
      setToastState({ ...toastState, open: true, severity: 'error', message: '没有可用 GPU' });
      return;
    }

    const body = {
      type: 'ssh_time',
      call_back_url: '',
      body: '',
      gpu_num: 1,
      batch_size: 1,
      device_cluster_type: 'single',
      extra_service_list: [],
      gpu_version: gpu.gpu_version,
      use_auth: true,
    };

    const result = await fetch(`${baseURL}/v1/user/missions/batch`, {
      method: 'POST',
      body: JSON.stringify(body),
      headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${token}` },
    });
    if (result.ok) {
      const { code, msg } = await result.json();
      if (code === 20000) {
        setToastState({ ...toastState, open: true, severity: 'success', message: '创建成功' });
        fetchRunningApps();
      } else {
        setToastState({ ...toastState, open: true, severity: 'error', message: msg });
      }
    }
  }

  async function closeApp(id) {
    const { confirmed } = await confirm({
      title: '关闭应用',
      description: '确定关闭该应用吗？',
      confirmationText: '确定',
      cancellationText: '取消',
    });
    if (confirmed) {
      const token = localStorage.getItem('token');
      const result = await fetch(`${baseURL}/v1/user/missions/close/batch`, {
        method: 'PUT',
        body: JSON.stringify({ ids: [id] }),
        headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${token}` },
      });
      if (result.ok) {
        const { code, msg } = await result.json();
        if (code === 20000) {
          setToastState({ ...toastState, open: true, severity: 'success', message: '关闭成功' });
          fetchRunningApps();
        } else {
          setToastState({ ...toastState, open: true, severity: 'error', message: msg });
        }
      }
    }
  }

  function formatISOTime(isoString) {
    if (isoString === '0001-01-01T08:00:00+08:00') return;

    // 1. 创建Date对象（自动处理时区）
    const date = new Date(isoString);

    // 2. 提取各时间组件
    const year = String(date.getFullYear()).padStart(4, '0');
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    const seconds = String(date.getSeconds()).padStart(2, '0');

    // 3. 拼接为标准格式
    return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
  }

  async function fetchRunningApps() {
    const token = localStorage.getItem('token');
    const { pageIndex: page_index, pageSize: page_size } = pagination;
    const params = { page_index, page_size, front_state: ['waiting', 'running'] };
    const temp = Object.entries(params)
      .map(([key, value]) => (Array.isArray(value) ? value.map((v) => `${key}=${v}`).join('&') : `${key}=${value}`))
      .join('&');

    const result = await fetch(`${baseURL}/v1/user/missions?${temp}`, {
      method: 'GET',
      headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${token}` },
    });
    console.log(result, 'result');
    if (result.ok) {
      const { code, data } = await result.json();
      if (code === 20000) {
        const { list, total } = data;
        const temp = (list || []).map((item) => {
          const { id, status, gpu_version, gpu_num, urls, mission_category, started_at, updated_at } = item;
          return {
            id,
            status,
            gpuVersion: `${gpu_version} * ${gpu_num}`,
            category: mission_category,
            startedAt: formatISOTime(started_at),
            updatedAt: formatISOTime(updated_at),
            url: urls ? JSON.parse(urls)[0] : '',
          };
        });
        setRunningApps(temp);
        setPagination({ ...pagination, total });
      }
      console.log(data, 'data');
    }
  }

  function logout() {
    localStorage.removeItem('token');
    setIsLogged(false);
  }

  function handleCloseToast(_e, reason) {
    if (reason === 'clickaway') {
      return;
    }

    setToastState({ ...toastState, open: false });
  }

  function handleChangePage(_e, newPage) {
    setPagination({ ...pagination, pageIndex: newPage + 1 });
  }

  function handleChangeRowsPerPage(e) {
    const pageSize = parseInt(e.target.value, 10);
    setPagination({ ...pagination, pageSize, pageIndex: 1 });
  }

  return (
    <>
      {isLogged ? (
        <div className="create-app">
          <div className="btns">
            <Button variant="contained" onClick={createSSHApp}>
              创建 SSH 应用
            </Button>
            <Button variant="contained" onClick={fetchRunningApps}>
              查询开启的应用
            </Button>
            <Button variant="contained" onClick={logout}>
              退出登录
            </Button>
          </div>

          <TableContainer component={Paper} className="table">
            <Table sx={{ minWidth: 650 }} aria-label="simple table">
              <TableHead>
                <TableRow>
                  {columns.map((col) => (
                    <TableCell align="center">{col.label}</TableCell>
                  ))}
                </TableRow>
              </TableHead>
              <TableBody>
                {runningApps.map((item) => (
                  <TableRow key={item.id}>
                    {columns.map((col) =>
                      col.key === 'action' ? (
                        <TableCell align="center">
                          <Button variant="text" onClick={() => closeApp(item.id)}>
                            关闭
                          </Button>
                        </TableCell>
                      ) : (
                        <TableCell align="center">{item[col.key] || '--'}</TableCell>
                      ),
                    )}
                  </TableRow>
                ))}
              </TableBody>
              {pagination.total > 0 && (
                <TableFooter>
                  <TableRow>
                    <TablePagination
                      labelRowsPerPage="每页显示条数"
                      rowsPerPageOptions={[10, 20, 50]}
                      colSpan={8}
                      count={pagination.total}
                      rowsPerPage={pagination.pageSize}
                      page={pagination.pageIndex - 1}
                      onPageChange={handleChangePage}
                      onRowsPerPageChange={handleChangeRowsPerPage}
                    />
                  </TableRow>
                </TableFooter>
              )}
            </Table>
          </TableContainer>
        </div>
      ) : !initLoading ? (
        <div className="login-form">
          <TextField value={phone} onChange={(e) => setPhone(e.target.value)} label="手机号" />
          <TextField type="password" value={password} onChange={(e) => setPassword(e.target.value)} label="密码" />
          <Button onClick={login}>登录</Button>
        </div>
      ) : null}

      <Snackbar
        open={toastState.open}
        anchorOrigin={{ vertical: 'top', horizontal: 'right' }}
        autoHideDuration={3000}
        onClose={handleCloseToast}
      >
        <Alert onClose={handleCloseToast} severity={toastState.severity} sx={{ width: '100%' }}>
          {toastState.message}
        </Alert>
      </Snackbar>
    </>
  );
}

export default App;
