classdef InheritCheckMx < PyMxClass
    methods
        function obj = InheritCheckMx()
            mod = pyimport_proxy('inherit_check_py');
            obj@PyMxClass(mod.PyMxExtend);
        end

        % virtual
        function out = pure(~, value)
            out = value;
            fprintf('ml.pure=%d\n', value);
        end

        % virtual
        function out = optional(~, value)
            out = 1000 * value;
            fprintf('ml.optional=%d\n', value);
        end

        % This is a concrete, non-virtual method.
        function [varargout] = dispatch(obj, varargin)
            varargout = cell(1, nargout);
            [varargout{:}] = obj.pyInvokeDirect('dispatch', varargin{:});
        end
    end
end
