<?php

namespace Toknot\Webui;

use FFI;
use FFI\CData;

class Webview
{
    const GTK_LIBS = [
        'gobject' => ['gobject.h', 'libgobject-2', '2.0'],
        'gio' => ['gio.h', 'libgio-2', '2.0'],
        'gtk' => ['gtk.h', 'libgtk-4', '4.0'],
        'webkit' => ['webkit.h', 'libwebkitgtk-6', '6.0'],
        'glib' => ['glib.h', 'libglib-2', '2.0']
    ];

    private static $libffi = [];
    public readonly string $appid;
    public string $windowTitle;
    private CData $webview;
    private static $reloadAction;
    private static $exitAction;
    private static $reopenAciton;
    public function __construct(string $title)
    {
        $this->appid = 'org.php.webview' . time();
        $this->windowTitle = $title;
        $this->window();
    }

    public function window()
    {
        $app = $this->gtk_application_new($this->appid, 0);
        $this->g_signal_connect($app, "activate", $this->webviewArea(...));
        $status = $this->g_application_run($app, 0, null);
        $this->g_object_unref($app);
        return $status;
    }

    public function webviewArea(CData $app, $userData)
    {
        $window = $this->gtk_application_window_new($app);

        $this->gtk_window_set_title($window, $this->windowTitle);
        $this->gtk_window_set_default_size($window, 1024, 768);

        $scrolled_window = $this->gtk_scrolled_window_new();
        $this->gtk_scrolled_window_set_policy($scrolled_window, 1, 1);
        $this->newExitAction($app);
        $this->newPHPReload();
        $this->newPHPReopen();
        // 创建 WebView
        $this->webview = $this->webkit_web_view_new();
        // 加载网页
        $this->webkit_web_view_load_html($this->webview, 'default', 'localhost');
        $this->g_signal_connect($this->webview, 'decide-policy', $this->decidePolicy(...), null);
        $webview_context = $this->webkit_web_context_get_default();

        $this->webkit_web_context_register_uri_scheme($webview_context, "php-exec", $this->php_scheme_request_cb(...), NULL, NULL);

        $webview_setting = $this->webkit_web_view_get_settings($this->webview);
        $this->g_object_set($webview_setting, "enable-developer-extras", TRUE, NULL);

        // $this->webkit->webkit_settings_set_allow_universal_access_from_file_urls($webview_setting, TRUE);
        // $this->webkit->webkit_settings_set_allow_file_access_from_file_urls($webview_setting, TRUE);
        // $this->g_signal_connect($this->webview, 'resource-load-started', $this->loadWebResouce(...));
        $this->g_signal_connect($this->webview, 'context-menu', $this->webviewContextMenu(...), $app);
        $this->gtk_scrolled_window_set_child($scrolled_window, $this->webview);
        $this->gtk_window_set_child($window, $scrolled_window);
        $this->gtk_window_present($window);
    }

    public function decidePolicy($webview, $decision, $decision_type, $data)
    {
        if ($decision_type === null) {
            $type = 0;
        } else {
            $type = self::$libffi['gobject']->cast('int*', FFI::addr($decision_type))[0];
        }
        $this->msg("type: $type");
        switch ($type) {
            case 0: //WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION
            case 1: //WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION
                $action = $this->webkit_navigation_policy_decision_get_navigation_action($decision);
                $actionType = $this->webkit_navigation_action_get_navigation_type($action);
                $this->msg('nav|window: action:' . $actionType);
                $request = $this->webkit_navigation_action_get_request($action);
                $uri = $this->webkit_uri_request_get_uri($request);
                $this->msg('nav:' . $uri);
                switch ($actionType) {
                    case 3:  //WEBKIT_NAVIGATION_TYPE_RELOAD
                        $this->webkit_policy_decision_ignore($decision);
                        $this->g_action_activate(self::$reloadAction, null);
                        return true;
                    case 0: //WEBKIT_NAVIGATION_TYPE_LINK_CLICKED
                        if (strpos($uri, '') !== false) {
                            $path = substr($uri, strlen(''));
                            if ($path[0] == '#') {
                                return true;
                            }
                        }
                        $urls = parse_url($uri);
                        if (is_dir($urls['path'])) {
                            $urls['path'] .= 'index.php';
                        }
                        $this->loadphp($urls['path'], $urls['query']);
                        $this->webkit_policy_decision_ignore($decision);
                        return true;
                    case 1: //WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED
                    case 2: //WEBKIT_NAVIGATION_TYPE_BACK_FORWARD
                    case 4: //WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED
                    case 5: //WEBKIT_NAVIGATION_TYPE_OTHER
                        //$this->webkit->webkit_policy_decision_ignore($decision);
                        break;
                }
                break;
            case 2: //WEBKIT_POLICY_DECISION_TYPE_RESPONSE
                $request = $this->webkit_response_policy_decision_get_request($decision);
                $uri = $this->webkit_uri_request_get_uri($request);
                $urls = parse_url($uri);
                if ($urls['scheme'] != 'php') {
                    return false;
                }
                $this->webkit_policy_decision_ignore($decision);
                if (is_dir($urls['path'])) {
                    $urls['path'] .= 'index.php';
                }
                $this->loadphp($urls['path'], $urls['query']);
                $this->msg('respose:' . $uri . ' | path:' . $urls['path']);
                return true;
        }

        return false;
    }

    public function php_scheme_request_cb($request, $data)
    {
        $uri = $this->webkit_uri_scheme_request_get_uri($request);
        $path = $this->webkit_uri_scheme_request_get_path($request);
        $method = $this->webkit_uri_scheme_request_get_http_method($request);
        $hdrs = $this->webkit_uri_scheme_request_get_http_headers($request);
        if ($method == 'POST') {
            $contentLen = $this->soup_message_headers_get_content_length($hdrs);
            if ($contentLen) {
                $stream = $this->webkit_uri_scheme_request_get_http_body($request);
            }
        }
        //$this->msg("PSRCB: $uri");
        $uris = parse_url($uri);
        $path = stripslashes(urldecode($path));
        if (str_starts_with($path, '/".')) {
            $path = substr($path, 3, -1);
        }
        $realpath =  $path;
        $type = mime_content_type($realpath);
        $isTxt = str_starts_with($type, 'text/');
        $ext = pathinfo($realpath, PATHINFO_EXTENSION);
        $contentType = 'text/html';
        if ($ext == 'php') {
            $this->loadPHPResource($request, $contentType, $realpath, $uris['query']);
        } else {
            if ($isTxt && $ext == 'js') {
                $contentType = 'text/javascript';
            } else if ($isTxt && $ext == 'css') {
                $contentType = 'text/css';
            } else {
                $contentType = $type;
            }
            $content = file_get_contents($realpath);
            $this->php_scheme_request_set_content($content, $request, $contentType);
        }
    }

    public function webviewContextMenu($webview, $menu, $hittest, $app)
    {
        $length = $this->webkit_context_menu_get_n_items($menu);
        for ($i = 0; $i < $length; $i++) {
            $item = $this->webkit_context_menu_get_item_at_position($menu, $i);
            if ($this->webkit_context_menu_item_is_separator($item)) {
                continue;
            }
            $stockaction = $this->webkit_context_menu_item_get_stock_action($item);
            if ($stockaction == self::$libffi['webkit']->WEBKIT_CONTEXT_MENU_ACTION_RELOAD) {
                $this->webkit_context_menu_remove($menu, $item);
                $itemRL = $this->webkit_context_menu_item_new_from_gaction(self::$reloadAction, '刷新页面', null);
                $this->webkit_context_menu_append($menu, $itemRL);
            }
        }
        $itemEX = $this->webkit_context_menu_item_new_from_gaction(self::$exitAction, '退出', null);
        $this->webkit_context_menu_prepend($menu, $itemEX);

        $itemRO = $this->webkit_context_menu_item_new_from_gaction(self::$reopenAciton, '重新打开', null);
        $this->webkit_context_menu_append($menu, $itemRO);
        $inspector = $this->webkit_web_view_get_inspector($webview);
        $this->webkit_web_inspector_show($inspector);
        return null;
    }

    public function newPHPReopen()
    {
        self::$reopenAciton = $this->g_simple_action_new('php-reopen', null);
        $this->g_signal_connect(self::$reopenAciton, 'activate', function () {
            $this->g_action_activate(self::$exitAction, null);
        });
    }

    public function newPHPReload()
    {
        self::$reloadAction = $this->g_simple_action_new('php-reload', null);
        $this->g_signal_connect(self::$reloadAction, 'activate', function () {
            //fwrite($this->reportFp, self::EXEC_RELOAD);
        });
    }

    public function newExitAction($app)
    {
        self::$exitAction = $this->g_simple_action_new('php-exit', null);
        $this->g_signal_connect(self::$exitAction, 'activate', function ($action, $param, $app) {
            $this->g_signal_emit_by_name($this->webview, 'destroy');
            $this->g_application_quit($app);
        }, $app);
    }

    public function g_signal_connect($ins, $signal, $handler, $data = null)
    {
        return $this->g_signal_connect_data($ins, $signal, $handler, $data, null, 0);
    }
    public function g_signal_connect_after($ins, $signal, $handler, $data = null)
    {
        return $this->g_signal_connect_data($ins, $signal, $handler, $data, null, 1);
    }

    public function findWebkitGTK(): array
    {
        $libpath = [];
        foreach (self::GTK_LIBS as $name => $info) {
            $libpath[$name] = $this->findDLL($info[1], $info[2]);
        }
        return $libpath;
    }

    public function findDLL(string $name, string $ver): string
    {
        $output = [];
        exec("ldconfig -p |grep $name", $output, $code);
        if ($code != 0) {
            throw new \RuntimeException("cant not find $name DLL, $ver");
        }
        list(, $path) = explode('=>', $output[0]);
        return realpath(trim($path));
    }

    public function initFFI()
    {
        $libs = $this->findWebkitGTK();
        foreach ($libs as $name => $lib) {
            self::$libffi[$name] = FFI::cdef(file_get_contents(__DIR__ . '/header/' . strtolower(PHP_OS_FAMILY) . '/' . self::GTK_LIBS[$name][0]), $lib);
        }
    }

    public function __call(string $name, array $arguments = [])
    {
        if (str_starts_with($name, 'gkt_')) {
            return self::$libffi['gtk']->$name(...$arguments);
        } else if (str_starts_with($name, 'webkit_')) {
            return self::$libffi['webkit']->$name(...$arguments);
        } else if (str_starts_with($name, 'g_application_')) {
            return self::$libffi['gio']->$name(...$arguments);
        } else if (str_starts_with($name, 'g_action_')) {
            return self::$libffi['gio']->$name(...$arguments);
        } else if (str_starts_with($name, 'g_simple_')) {
            return self::$libffi['gio']->$name(...$arguments);
        } else if (str_starts_with($name, 'g_main_')) {
            return self::$libffi['glib']->$name(...$arguments);
        } else if (str_starts_with($name, 'g_idle_')) {
            return self::$libffi['glib']->$name(...$arguments);
        } else if (str_starts_with($name, 'g_object_')) {
            return self::$libffi['gobject']->$name(...$arguments);
        } else if (str_starts_with($name, 'g_signal_')) {
            return self::$libffi['gobject']->$name(...$arguments);
        }
        if (
            $name == 'g_variant_type_is_basic' ||
            $name == 'g_memory_input_stream_new_from_data'
        ) {
            return self::$libffi['gio']->$name(...$arguments);
        }
        if (
            $name == 'g_source_remove' ||
            $name == 'g_error_new' ||
            $name == 'g_uri_unescape_string'
        ) {
            return self::$libffi['glib']->$name(...$arguments);
        }
        if ($name == 'g_clear_object') {
            return self::$libffi['gobject']->$name(...$arguments);
        }
        throw new \BadFunctionCallException("call C function $name not exits");
    }
}
